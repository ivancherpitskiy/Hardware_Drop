#include "server.h"
#include "globals.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <pthread.h>

static const char *s_root_dir = "./shared_files";

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        // --- 0. Оновлюємо стан пам'яті для екрана та сторінки ---
        struct statvfs stat;
        double total_gb = 0, free_gb = 0, used_gb = 0;
        int percent = 0;
        
        if (statvfs(s_root_dir, &stat) == 0 && stat.f_blocks > 0) {
            total_gb = (double)(stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);
            free_gb = (double)(stat.f_bfree * stat.f_frsize) / (1024 * 1024 * 1024);
            used_gb = total_gb - free_gb;
            if (total_gb > 0) {
                percent = (int)((used_gb / total_gb) * 100);
            }
            
            // Передаємо на екран
            pthread_mutex_lock(&state_mutex);
            app_state.free_space_percent = percent;
            pthread_mutex_unlock(&state_mutex);
        }

        // --- 1. ГОЛОВНА СТОРІНКА ---
        if (mg_match(hm->uri, mg_str("/"), NULL)) {
            char html[8192];
            int len = 0;
            
            // Твій старий перевірений HTML + кнопка пошуку
            len += snprintf(html + len, sizeof(html) - len, 
                "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<meta charset='utf-8'><title>Hardware Drop</title>"
                "<style>body{font-family:sans-serif; background:#1a1a1a; color:#eee; padding:20px; max-width:500px; margin:auto;}"
                ".card{background:#333; padding:20px; border-radius:12px; margin-bottom:20px; box-shadow:0 4px 10px rgba(0,0,0,0.5);}"
                "a{color:#4CAF50; text-decoration:none; font-weight:bold; float:right; background:#222; padding:5px 15px; border-radius:5px;}"
                "li{padding:12px; border-bottom:1px solid #444; list-style:none; font-size:16px;}"
                "input[type=file]{margin-bottom:15px; width:100%%;}"
                "button{background:#007bff; color:white; border:none; padding:12px; width:100%%; border-radius:6px; font-size:16px; cursor:pointer; font-weight:bold;}"
                ".btn-find{background:#f44336; margin-bottom:15px;}"
                ".progress-bg{background:#555; border-radius:10px; height:15px; width:100%%; margin-top:10px; overflow:hidden;}"
                ".progress-bar{background:#4CAF50; height:100%%;}"
                "</style></head><body>"
                
                "<div class='card'>"
                "<form action='/find' method='POST'><button type='submit' class='btn-find'>🔊 Знайти пристрій</button></form>"
                "<h2>💾 Стан пам'яті</h2>"
                "<p style='margin:0 0 10px 0; font-size:14px; color:#bbb;'>Використано: %.2f GB / %.2f GB (Вільних: %.2f GB)</p>"
                "<div class='progress-bg'><div class='progress-bar' style='width: %d%%;'></div></div>"
                "</div>"

                "<div class='card'><h2>⬆️ Завантажити на Raspberry</h2>"
                "<form action='/upload' method='POST' enctype='multipart/form-data'>"
                "<input type='file' name='file' required>"
                "<button type='submit'>Відправити файл</button>"
                "</form></div>"
                "<div class='card'><h2>📂 Файли на пристрої</h2><ul style='padding:0; margin:0;'>",
                used_gb, total_gb, free_gb, percent
            );

            DIR *dir = opendir(s_root_dir);
            if (dir) {
                struct dirent *dp;
                while ((dp = readdir(dir)) != NULL) {
                    if (dp->d_name[0] == '.') continue;
                    len += snprintf(html + len, sizeof(html) - len, 
                        "<li>📄 %s <a href='/%s' download>Скачати</a></li>", 
                        dp->d_name, dp->d_name);
                }
                closedir(dir);
            }
            
            snprintf(html + len, sizeof(html) - len, "</ul></div></body></html>");
            mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", html);
        } 
        
        // --- 2. ПОШУК ПРИСТРОЮ ---
        else if (mg_match(hm->uri, mg_str("/find"), NULL)) {
            pthread_mutex_lock(&state_mutex);
            app_state.find_device_alarm = true;
            pthread_mutex_unlock(&state_mutex);
            mg_http_reply(c, 302, "Location: /\r\n", "");
        }

        // --- 3. ОБРОБКА ПРИЙОМУ ФАЙЛУ ---
        else if (mg_match(hm->uri, mg_str("/upload"), NULL)) {
            pthread_mutex_lock(&state_mutex);
            app_state.is_uploading = true;
            pthread_mutex_unlock(&state_mutex);

            struct mg_http_part part;
            size_t ofs = 0;
            while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
                if (part.filename.len > 0) {
                    char path[256];
                    snprintf(path, sizeof(path), "%s/%.*s", s_root_dir, (int) part.filename.len, part.filename.buf);
                    FILE *fp = fopen(path, "wb");
                    if (fp != NULL) {
                        fwrite(part.body.buf, 1, part.body.len, fp);
                        fclose(fp);
                    }
                }
            }
            
            pthread_mutex_lock(&state_mutex);
            app_state.is_uploading = false;
            pthread_mutex_unlock(&state_mutex);

            mg_http_reply(c, 302, "Location: /\r\n", "");
        } 
        
        // --- 4. РОЗДАЧА ФАЙЛІВ (Той самий робочий код) ---
        else {
            struct mg_http_serve_opts opts = { .root_dir = s_root_dir };
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

// Головний цикл потоку
void *server_thread_loop(void *arg) {
    (void)arg;
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    
    if (mg_http_listen(&mgr, "http://0.0.0.0:8000", fn, NULL) == NULL) {
        printf("[NET] Failed to start server.\n");
        return NULL;
    }

    printf("[NET] Server started on port 8000.\n");
    
    while (1) {
        mg_mgr_poll(&mgr, 100);
    }
    
    mg_mgr_free(&mgr);
    return NULL;
}