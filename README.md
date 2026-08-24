# Hardware Drop

Локальний сервіс для обміну файлами на базі Raspberry Pi. 
Проєкт розроблено як демонстрацію навичок для Ajax Embedded Engineering Internship.


## Опис

Hardware Drop — це Linux-демон, написаний на C, який перетворює Raspberry Pi на автономну точку локального обміну файлами. Пристрій самостійно створює Wi-Fi Hotspot, піднімає веб-сервер для прийому файлів і виводить статус роботи на OLED-дисплей.

### Основні можливості
* Автоматичне розгортання Wi-Fi точки доступу через NetworkManager.
* Взаємодія з I2C OLED-дисплеєм (SSD1306) для виведення QR-коду та поточної IP-адреси.
* Локальний веб-сервер на базі Mongoose для збереження файлів.
* Інтеграція з systemd для автоматичного запуску та роботи у фоновому режимі.

## Технології
* **Мова:** C11
* **Платформа:** Raspberry Pi (Raspberry Pi OS)
* **Мережа та системні API:** NetworkManager, D-Bus, systemd
* **Бібліотеки:** Mongoose, кастомний I2C-драйвер екрана

## Структура репозиторію

```text
├── src/                  # Вихідний код проєкту (.c, .h, Makefile)
├── assets/               # Фотографії та скріншоти
└── README.md             # Документація проєкту
```

##  Збірка та запуск
1. Клонування:
```
git clone https://github.com/ivancherpitskiy/Hardware_Drop.git
cd Hardware_Drop/src
```
2. Збірка:

```
make
```
3. Налаштування I2C (якщо не увімкнено):

```
sudo raspi-config nonint do_i2c 0
```
4. Запуск:
```
Bash
sudo ./storage_daemon
```
