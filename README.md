# Операционная система

Простая операционная система. Представляет из себя интерактивный словарь с английского языка на финский.
Компиляция происходит с помощью FASM, Microsoft Visual Studio C/C++ Compiler

Используемые команды:
FASM.EXE bootsect.asm
cl /GS- /c kernel.cpp
link /OUT:kernel.bin /BASE:0x10000 /FIXED /FILEALIGN:512 /MERGE:.rdata=.data /IGNORE:4254 /NODEFAULTLIB /ENTRY:kmain /SUBSYSTEM:NATIVE kernel.obj
Операционную систему можно запустить, например, с помощью qemu, используя команду
qemu-system-x86_64.exe -fda bootsect.bin -fdb kernel.bin

Загрузчик после короткой самоинициализации и загрузки ядра выводит на экран строку, состоящую из _ . Далее пользователь с помощью клавиатуры может выбрать, слова на какие буквы его будут интересовать в словаре. Например:

"abcde_____kl_nop____uv_x_z"

после нажатия на клавишу 'w'

"abcde_____kl_nop____uvwx_z"

При нажатии на Enter заканчивается выбор интересующих слов и производится окончательная инициализация системы.

Словарь хранит слова в секции данных ядра. При запуске системы он сортируются. Также при запуске происходит окончательная инициализация системы. 

Интерфейс словаря:

  info - выводит информацию об авторе и средствах разработки ОС, а также перечень выбранных букв.
  
  dictinfo - выводит информацию о загруженном словаре (язык, общее кол-во слов, количество доступных для перевода слова)
  
  translate <слово> - переводит набранное слово на финский язык
  
  wordstat <буква> - выводит количество загруженных слов в словаре на указанную букву.
  
  anyword <буква> - выводит на экран случайное слово на указанную букву, если буква не указана, то выводит случайное слово.
  
  clear - очистка экрана
  
  shutdown - выключение системы
  

Для работы системы были реализованы:

  Генератор псевдослучайных чисел, использующий алгоритм ЛКГ
  
  Обработчики прерываний таймера и клавиатуры
  
  Поддержка как заглавных, так и строчных букв латинского алфавита
  
  Правильное отображение букв, специфичных для финкского языка
  
  Промотка консоли, при достижениии края активной строкой
