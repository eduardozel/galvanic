idf.py set-target esp32c3
rem idf.py menuconfig
rem idf.py -p COM3 flash monitor
idf.py fullclean
"C:\distrib\putty-0.73-ru-17-portable\PuTTY PORTABLE\putty_portable.exe" -serial COM3 -sercfg 115200,8,n,1,N
pause