use16
org 0x7C00
start: 
xor ax,ax       ; Обнуление регистра
mov ds,ax       ; Заполнение регистра сегмента данных
mov es,ax       ; Заполнение регистра дополнительного сегмента
mov ss,ax       ; Заполнение стекового регистра
mov sp, start   ; Выставление указателя на стек

; Загрузка кода ядра
mov ax,0x1100 ; Загрузка будет происходить по адресу 11000h
mov es,ax 
mov bx,0 ; Выставление адреса загрузки
mov ah,2 ; Выставляется номер функции
mov dl,1 ; Используется второй диск
mov dh,0 ; Нулевая головка диска
mov cl,2 ; Номер сектора, с которого начинается загрузка(1-0 | 2-200 | 3-400 | 4-600 | 5-800 | 6-A00 | 7-C00 | 8-E00 |
;9-1000 | 10-1200 | 11-1400 | 12-1600 | 13-1800 | 14-1A00 | 15-1C00 | 16-1E00 | 17-2000 | 18 - 2200 | 19-2400 | 20-2600 | 21-2800)
mov ch,0 ; Выбирается нулевой цилиндр
mov al,18 ; Количество загружаемых секторов (<=18)(0-0 | 1-200 | 2-400 | 3-600 | 4-800 | 5-A00 | 6-C00 | 7-E00 | 8-1000 
;| 9-1200 10-1400 | 11-1600 | 12-1800 | 13-1A00 | 14-1C00 | 15-1E00 || 16 - 2000 | 17-2200 | 18-2400)
int 0x13

;Так как загружаемый блок больше 18 секторов, то приходится использовать два вызова функции для одной части кода
mov ax,0x1340
mov es,ax
mov bx,0
mov ah,2
mov dl,1 
mov dh,0   
mov cl,20   ;;;;;;;;;;
mov ch,0  
mov al,1   ;;;;;;;;;;
int 0x13

mov ax,0x1400
mov es,ax
mov bx,0
mov ah,2
mov dl,1 
mov dh,0   
mov cl,21  ;;;;;;;;;;
mov ch,0  
mov al,11   ;;;;;;;;;;
int 0x13
; Очистка экрана
mov ax, 0x0003
int 0x10
; Заполнение экрана символами '_' на 26 символов
mov ax, 0xb800; Работа напрямую с видеобуфером
mov ds, ax  ;Именение сегментного регистра ds. По умолчанию [ax] значит взятие ячейки по адресу ds:ax
mov cx, 0x1a
xor di, di
filling_:
mov ah, '_'
mov [di], ah
inc di
mov ah, 0x07
mov [di], ah
inc di
loop filling_ ;loop = dec cx + jmp filling_
; Обработка нажатия кнопки
button_pressing:
mov ah, 0x0
int 0x16
cmp al, 13; Если нажатая клавиша-enter то переходим к переходу в защищенный режим
jz ending
mov bh, 'a'; Если нажатая кнопка не является маленькой буквой латинского алфавита, то ничего не делаем
dec bh
cmp al, bh
jna button_pressing 
cmp al, 'z'
ja button_pressing; Если все проверки пройдены то инвертируем содержимое ячейки, отвечающей нажатой букве.
xor bx, bx 
mov bl, al
mov di, bx
sub di, 'a';Получаем индекс с помощью вычитания числа == символу 'a'
imul di, 2 ; Умножаем на 2, тк второй байт - цвет ячейки
mov bx, [di]
cmp bl, al
jz next_button_pressing
mov [di], al
jmp button_pressing
next_button_pressing:
mov bl, '_'
mov [di], bl
jmp button_pressing

ending:
xor ax,ax ; Восстановление ds
mov ds, ax
in al, 0x92 ;Включение адресной линии А20
or al, 2
out 0x92, al

lgdt [gdt_info]  ;Запись gdt_info в GDTR
cli ;Отключение прерываний

mov eax, cr0 ;Переход в защищенный режим
or al, 1
mov cr0, eax

jmp 0x8:protected_mode

use32
protected_mode:
    mov ax, 0x10
    mov es, ax
    mov ds, ax
    mov ss, ax
    call KERNEL
KERNEL:
    call 0x11000

gdt:  ; Создание глобальной таблицы дескрипторов
    db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    db 0xff, 0xff, 0x00, 0x00, 0x00, 0x9a, 0xcf, 0x00
    db 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xcf, 0x00
gdt_info:
    dw gdt_info - gdt
    dw gdt, 0
    
times(512- ($-start)-2) db 0
db 0x55, 0xAA
