extern "C" int kmain();
__declspec(naked) void startup()
{
	__asm {
        call kmain;
	}
}

#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)

// Селектор секции кода, установленный загрузчиком ОС
#define GDT_CS (0x8)
#define PIC1_PORT (0x20)
#define VIDEO_BUF_PTR (0xb8000)
#define IDT_TYPE_MY (0x21)
#define KEY_BUFFER_SIZE 4
#define DICT_LEN 125
void out_str(int color, const char *ptr, unsigned int strnum, int pos);
#pragma pack(push, 1)
int color;
int leng_buf;
char buffer[41];
int str_pos;
char *dict[DICT_LEN];
int letters[26];
char *usable_words[DICT_LEN];
int loaded_words;
int ticks;
int caps_lock;
int pos;
unsigned char loaded_letters[26];

struct idt_entry
{
	unsigned short base_lo;  // Младшие биты адреса обработчика
	unsigned short segm_sel; // Селектор сегмента кода
	unsigned char always0;   // Этот байт всегда 0
	unsigned char flags;	 // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
	unsigned short base_hi;  // Старшие биты адреса обработчика
};

// Структура, адрес которой передается как аргумент команды lidt
struct idt_ptr
{
	unsigned short limit;
	unsigned int base;
};

#pragma pack(pop)

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp;

__declspec(naked) void default_intr_handler()
{
	__asm {   pusha }

	__asm
	{
		popa
			iretd
	}
}

typedef void (*intr_handler)();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
	unsigned int hndlr_addr = (unsigned int)hndlr;

	g_idt[num].base_lo = (unsigned short)(hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short)(hndlr_addr >> 16);
}

void intr_init()
{
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);

	for (int i = 0; i < idt_count; i++)
	{
		intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
	}
}

void intr_start()
{
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);

	g_idtp.base = (unsigned int)(&g_idt[0]);
	g_idtp.limit = (sizeof(struct idt_entry) * idt_count) - 1;

	__asm {
		lidt g_idtp
	}

	//__lidt(&g_idtp);
}

void intr_enable()
{
	__asm sti;
}
void intr_disable()
{
	__asm cli;
}

__inline unsigned char inb(unsigned short port)
{ // Чтение из порта
	unsigned char data;
	__asm {
		push dx
		mov dx, port
		in al, dx
		mov data, al
		pop dx
	}
	return data;
}

__inline void outw(unsigned short port, unsigned short data)
{ // Запись
	__asm {
		push dx
		mov dx, port
		mov ax, data
		out dx, ax
		pop dx
	}
}

__inline void outb(unsigned short port, unsigned char data)
{ // Запись
	__asm {
		push dx
		mov dx, port
		mov al, data
		out dx, al
		pop dx
	}
}

void out_str(int color, const char *ptr, unsigned int strnum, int pos)
{
	unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;

	video_buf += 80 * 2 * strnum + pos * 2;

	while (*ptr)
	{
		video_buf[0] = (unsigned char)*ptr; // Символ (код)
		video_buf[1] = color;				// Цвет символа и фона

		video_buf += 2;
		ptr++;
	}
}

// Базовый порт управления курсором текстового экрана.
//Подходит для большинства, но может отличаться в других BIOS и в общем случае адрес должен быть прочитан из BIOS data area.
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина текстового экрана

// Функция переводит курсор на строку strnum (0 – самая верхняя) в позицию pos на этой строке (0 – самое левое положение).
void cursor_moveto(unsigned int strnum, unsigned int pos)
{
	unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
	outb(CURSOR_PORT, 0x0F);
	outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
	outb(CURSOR_PORT, 0x0E);
	outb(CURSOR_PORT + 1, (unsigned char)((new_pos >> 8) & 0xFF));
}

int cmp(char *str, char *check, int n)
{
	//Сравнение строк
	for (int i = 0; i < n; ++i)
	{
		if (str[i] != check[i])
		{
			return 0;
		}
	}
	return 1;
}

void erase_buf()
{ //очистка буфера ввода
	for (int i = 0; i < 41; ++i)
	{
		buffer[i] = '\0';
	}
	leng_buf = 0;
}

void clear()
{ //очистка экрана
	for (int i = 0; i < 80; i++)
	{
		for (int j = 0; j < 25; j++)
		{
			out_str(color, " ", j, i);
		}
	}
	str_pos = 0;
	pos = 0;
}

void loop()
{//Бесконечный цикл
	while (1)
	{
		__asm {
			hlt
		}
	}
}

void check_rows()
{//Функция проверяет, дошел ли вывод до конца экрана и прокручивает его в этом случае
	if (str_pos > 23)
	{
		unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
		for (int i = 160; i < 160 * (str_pos + 1); i++)
		{
			video_buf[i - 160] = video_buf[i];
		}
		str_pos--;
	}
}

void next_row()
{//Функция переноса строки
	str_pos++;
	intr_disable();
	check_rows();
	intr_enable();
}

void start_input()
{//Функция очищает текущий буфер и перемещает курсор на начало строки. Представляет из себя подготовку к пользовательскому вводу
	erase_buf();
	pos = 0;
	cursor_moveto(str_pos, pos);
}

#define RAND_MAX 32767

static unsigned long int next = 1;

int rand(void)
{//Рандомные числа вычисляются с помощью ЛКГ
	next = next * 1103515245 + 12345;
	return (unsigned int)(next / 65536) % (RAND_MAX + 1);
}

void srand(unsigned int seed)
{
	next = seed;
}

int strcmp(const char *s1, const char *s2)
{//Функция сравнения строк
	while (*s1 && (*s1 == *s2))
		s1++, s2++;
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void dict_sort()
{//Сортировка словаря
	int min = 0;
	for (int i = 0; i < DICT_LEN - 1; i++)
	{
		min = i;
		for (int j = i + 1; j < DICT_LEN; j++)
		{
			if (strcmp(dict[j], dict[min]) < 0)
				min = j;
		}
		char *temp = dict[i];
		dict[i] = dict[min];
		dict[min] = temp;
	}
}

void out_number(int number)
{//Вывод числа
	char buff[40];
	int now = 39;
	while (number > 0)
	{
		buff[now] = (number % 10) + '0';
		number /= 10;
		now--;
	}
	int i = 0;
	for (now += 1; now < 40; i++, now++)
	{
		buff[i] = buff[now];
	}
	buff[i] = '\0';
	out_str(color, buff, str_pos, pos);
}
void error()
{//Вывод сообщения о неправильной команде
	next_row();
	pos = 0;
	out_str(color, "    Unknown command", str_pos, pos);
	next_row();
	erase_buf();
	cursor_moveto(str_pos, pos);
	return;
}

int bin_search(int left, int right, char *goal)
{//Бинарный поиск в словаре
	int median = -1;
	while (left < right - 1)
	{
		median = (left + right) / 2;
		if (strcmp(dict[median], goal) < 0)
		{
			left = median;
		}
		else
		{
			right = median;
		}
	}
	return right;
}

void on_key(unsigned char scan_code)
{//Обработчик нажатия клавиши
	int n = (int)scan_code;
	int bad_flag = 0;//Флаг нажатия недопустимой клавиши
	static int was_srand = 0; // Флаг показывающий, был ил инициализирован ЛКГ
	char res_symb[3];
	res_symb[1] = '\0';
	switch (n)
	{
	case 2:
		res_symb[0] = '1';
		break;
	case 3:
		res_symb[0] = '2';
		break;
	case 4:
		res_symb[0] = '3';
		break;
	case 5:
		res_symb[0] = '4';
		break;
	case 6:
		res_symb[0] = '5';
		break;
	case 7:
		res_symb[0] = '6';
		break;
	case 8:
		res_symb[0] = '7';
		break;
	case 9:
		res_symb[0] = '8';
		break;
	case 10:
		res_symb[0] = '9';
		break;
	case 11:
		res_symb[0] = '0';
		break;
	case 52:
		res_symb[0] = '.';
		break;
	case 53:
		res_symb[0] = '/';
		break;
	case 12:
		res_symb[0] = '-';
		break;
	case 16:
		res_symb[0] = caps_lock ? 'Q' : 'q';//В зависимости от флага caps_lock выбирается либо заглавная, либо строчная буква.
		break;
	case 17:
		res_symb[0] = caps_lock ? 'W' : 'w';
		break;
	case 18:
		res_symb[0] = caps_lock ? 'E' : 'e';
		break;
	case 19:
		res_symb[0] = caps_lock ? 'R' : 'r';
		break;
	case 20:
		res_symb[0] = caps_lock ? 'T' : 't';
		break;
	case 21:
		res_symb[0] = caps_lock ? 'Y' : 'y';
		break;
	case 22:
		res_symb[0] = caps_lock ? 'U' : 'u';
		break;
	case 23:
		res_symb[0] = caps_lock ? 'I' : 'i';
		break;
	case 24:
		res_symb[0] = caps_lock ? 'O' : 'o';
		break;
	case 25:
		res_symb[0] = caps_lock ? 'P' : 'p';
		break;
	case 30:
		res_symb[0] = caps_lock ? 'A' : 'a';
		break;
	case 31:
		res_symb[0] = caps_lock ? 'S' : 's';
		break;
	case 32:
		res_symb[0] = caps_lock ? 'D' : 'd';
		break;
	case 33:
		res_symb[0] = caps_lock ? 'F' : 'f';
		break;
	case 34:
		res_symb[0] = caps_lock ? 'G' : 'g';
		break;
	case 35:
		res_symb[0] = caps_lock ? 'H' : 'h';
		break;
	case 36:
		res_symb[0] = caps_lock ? 'J' : 'j';
		break;
	case 37:
		res_symb[0] = caps_lock ? 'K' : 'k';
		break;
	case 38:
		res_symb[0] = caps_lock ? 'L' : 'l';
		break;
	case 44:
		res_symb[0] = caps_lock ? 'Z' : 'z';
		break;
	case 45:
		res_symb[0] = caps_lock ? 'X' : 'x';
		break;
	case 46:
		res_symb[0] = caps_lock ? 'C' : 'c';
		break;
	case 47:
		res_symb[0] = caps_lock ? 'V' : 'v';
		break;
	case 48:
		res_symb[0] = caps_lock ? 'B' : 'b';
		break;
	case 49:
		res_symb[0] = caps_lock ? 'N' : 'n';
		break;
	case 50:
		res_symb[0] = caps_lock ? 'M' : 'm';
		break;
	case 57:
		res_symb[0] = ' ';
		break;
	case 28:
		res_symb[0] = 'e';
		res_symb[1] = 'n';
		res_symb[2] = '\0';
		break;
	case 14:
		res_symb[0] = 'b';
		res_symb[1] = 'c';
		res_symb[2] = '\0';
		break;
	case 58:
		bad_flag = 1;
		caps_lock = !caps_lock;
		break;
	default:
		bad_flag = 1;
		res_symb[0] = (char)n;
	}

	if (n != 28 && n != 14 && n != 42 && leng_buf < 40 && bad_flag == 0)
	{//При обычном нажатии допустимой клавиши она добавляется на экран
		res_symb[1] = '\0';
		out_str(color, res_symb, str_pos, pos);
		buffer[leng_buf++] = res_symb[0];
		cursor_moveto(str_pos, ++pos);
		return;
	}
	else if (n == 14 && leng_buf > 0) //Удаление
	{
		pos--;
		cursor_moveto(str_pos, pos);
		out_str(color, " ", str_pos, pos);
		buffer[leng_buf] = '\0';
		leng_buf--;
		return;
	}
	else if (n == 28)
	{
		if (cmp(buffer, "clear", 5) == 1 && leng_buf == 5)
		{ //Очистка
			clear();
			cursor_moveto(str_pos, pos);
			erase_buf();
		}
		else if (cmp(buffer, "info", 4) == 1 && leng_buf == 4)
		{//Вывод информации
			next_row();
			out_str(color, "DictOS: v.1 Developer: Dostanich Daniel, 23656/5, SpbPU, 2019", str_pos, pos);
			next_row();
			out_str(color, "Compilers: bootloader: FASM | kernel: MS C Compiler", str_pos, pos);
			next_row();
			out_str(color, "Loaded letters:", str_pos, pos);
			next_row();
			out_str(color, (char *)loaded_letters, str_pos, pos);
			next_row();
			start_input();
		}
		else if (cmp(buffer, "shutdown", 8) == 1 && leng_buf == 8)
		{ //Выключение
			next_row();
			pos = 4;
			out_str(color, "Powering off", str_pos, pos);
			outw(0x604, 0x2000);
			return;
		}
		else if (cmp(buffer, "dictinfo", 8) == 1 && leng_buf == 8)
		{//Вывод информации о словаре
			next_row();
			pos = 4;
			out_str(color, "Dictionary: en -> fi", str_pos, pos);
			next_row();
			out_str(color, "Total number of words:", str_pos, pos);
			pos = 27;
			out_number(DICT_LEN);
			next_row();
			pos = 4;
			out_str(color, "Number of loaded words:", str_pos, pos);
			pos = 28;
			out_number(loaded_words);
			next_row();
			start_input();
		}
		else if (cmp(buffer, "translate ", 10) == 1)
		{//Перевод
			next_row();
			pos = 4;
			char *word = buffer + 10;
			//Если буква не была загружна
			if (loaded_letters[buffer[10] - 'a'] == '_')
			{
				out_str(color, "Error: word is unknown", str_pos, pos);
			}
			//Если первая буква - а
			else if (buffer[10] == 'a')
			{
				//Выполняется бинарный поиск. Левая граница берется равной -1. Это единственное отличие от реализации для других букв
				int res = bin_search(-1, letters[0], word);
				//Искомое слово либо текущее либо предыдущее
				char *s = dict[res];
				while (*s && (*s == *word))
					s++, word++;
				if (*s == ' ' && *word == '\0')
				{
					out_str(color, dict[res], str_pos, pos);
				}
				else
				{
					s = dict[res - 1];
					while (*s && (*s == *word))
						s++, word++;
					if (*s == ' ' && *word == '\0')
					{
						out_str(color, dict[res], str_pos, pos);
					}
					else
					{
						out_str(color, "Error: word is unknown", str_pos, pos);
					}
				}
			}
			else
			{
				int res = bin_search(letters[buffer[10] - 'a' - 1] - 1, letters[buffer[10] - 'a'], word);
				char *s = dict[res];
				while (*s && (*s == *word))
					s++, word++;
				if (*s == ' ' && *word == '\0')
				{
					out_str(color, dict[res], str_pos, pos);
				}
				else
				{
					s = dict[res - 1];
					while (*s && (*s == *word))
						s++, word++;
					if (*s == ' ' && *word == '\0')
					{
						out_str(color, dict[res - 1], str_pos, pos);
					}
					else
					{
						out_str(color, "Error: word is unknown", str_pos, pos);
					}
				}
			}
			next_row();
			start_input();
		}
		else if (cmp(buffer, "wordstat ", 9) == 1 && leng_buf == 10)
		{//Статистика слова на выбранную букву
			if ((buffer[9] < 'a') || (buffer[9] > 'z'))
			{
				error();
				return;
			}
			pos = 4;
			next_row();
			out_str(color, "Words loaded : ", str_pos, pos);
			pos = 19;
			if (loaded_letters[buffer[9] - 'a'] == '_')
			{
				out_str(color, "0", str_pos, pos);
			}
			else if (buffer[9] == 'a')
			{
				out_number(letters[0]);
			}
			else
			{
				out_number(letters[buffer[9] - 'a'] - letters[buffer[9] - 'a' - 1]);
			}
			next_row();
			start_input();
		}
		else if (cmp(buffer, "anyword ", 7) == 1 && leng_buf == 7)
		{//Вывод случайной буквы
			pos = 4;
			if (was_srand == 0)
			{
				srand(ticks);
				was_srand = 1;
			}
			next_row();
			if (loaded_words == 0)
			{
				out_str(color, "Error: no words", str_pos, pos);
			}
			else
			{
				int random = rand() % loaded_words;
				out_str(color, usable_words[random], str_pos, pos);
			}
			next_row();
			start_input();
		}
		else if (cmp(buffer, "anyword ", 7) == 1)
		{//Вывод случайного слова на случайную букву
			if (was_srand == 0)
			{
				srand(ticks);
				was_srand = 1;
			}
			next_row();
			pos = 4;
			if ((buffer[8] < 'a') || (buffer[8] > 'z') || (loaded_letters[buffer[8] - 'a'] == '_'))
			{
				out_str(color, "Error: no words", str_pos, pos);
			}
			else
			{
				int total = buffer[8] == 'a' ? (letters[0] + 1) : (letters[buffer[8] - 'a'] - letters[buffer[8] - 'a' - 1]);
				int random = rand() % total;
				buffer[8] == 'a' ? out_str(color, dict[random], str_pos, pos) : out_str(color, dict[letters[buffer[8] - 'a' - 1] + 1 + random], str_pos, pos);
			}
			next_row();
			start_input();
		}
		else
		{
			next_row();
			pos = 0;
			out_str(color, "    Unknown command", str_pos, pos);
			next_row();
			erase_buf();
			cursor_moveto(str_pos, pos);
			return;
		}
	}
	else if (n == 42)
	{
		res_symb;
		return;
	}
	return;
}

void keyb_process_keys()
{ // Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
	if (inb(0x64) & 0x01)
	{
		unsigned char scan_code;
		unsigned char state;

		scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры

		if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
			on_key(scan_code);
	}
}

__declspec(naked) void ticks_handler()
{
	__asm pusha;
	ticks++;
	outb(PIC1_PORT, 0x20);
	__asm {
		popa
		iretd
	}
}
__declspec(naked) void keyb_handler()
{
	__asm pusha;

	// Обработка поступивших данных
	keyb_process_keys();

	// Отправка контроллеру 8259 нотификации о том, что прерывание обработано.
	//Если не отправлять нотификацию, то контроллер не будет посылать новых сигналов о прерываниях до тех пор, пока ему не сообщать что прерывание обработано.
	outb(PIC1_PORT, 0x20);

	__asm {
		popa
		iretd
	}
}

void keyb_ticks_init()
{
	// Регистрация обработчика прерывания
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
	intr_reg_handler(0x08, GDT_CS, 0x80 | IDT_TYPE_INTR, ticks_handler);
	// segm_sel=0x8, P=1, DPL=0, Type=Intr
	// Разрешение только прерываний клавиатуры от контроллера 8259
	outb(PIC1_PORT + 1, 0xFF ^ 0x03); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура).
									  // Разрешены будут только прерывания, чьи биты установлены в 0
}

void load_dict()
{
	char current_letter = 'a';
	int iterator = 0;
	int previous = 0;
	int usable_words_iter = 0;
	for (int i = 0; i < 26; i++)
	{
		while (iterator < DICT_LEN && dict[iterator][0] == current_letter)
		{
			if(loaded_letters[i] != '_'){
				usable_words[usable_words_iter] = dict[iterator];
				usable_words_iter++;
				loaded_words++;
			}
			iterator++;
		}
		previous = iterator - 1;
		letters[i] = iterator - 1;
		current_letter++;
	}
}

extern "C" int kmain()
{
	dict[0] = "air ilma";
	dict[1] = "area alue";
	dict[2] = "act toimia";
	dict[3] = "arise nousta";
	dict[4] = "about noin";
	dict[5] = "beer olut";
	dict[6] = "be olla";
	dict[7] = "bane myrkky";
	dict[8] = "boss p\x94s\x94";
	dict[9] = "bold selv\x84";
	dict[10] = "can t\x94lkki";
	dict[11] = "course rata";
	dict[12] = "cent sentti";
	dict[13] = "cow lehm\x84";
	dict[14] = "care hoito";
	dict[15] = "deer hirvi";
	dict[16] = "day p\x84iv\x84";
	dict[17] = "dry kuiva";
	dict[18] = "dust p\x94ly";
	dict[19] = "dog koira";
	dict[20] = "eat sy\x94 d\x84";
	dict[21] = "ear korva";
	dict[22] = "east it\x84";
	dict[23] = "enter astua";
	dict[24] = "every joka";
	dict[25] = "forest mets\x84";
	dict[26] = "fire palo";
	dict[27] = "farm maatila";
	dict[28] = "fine hyv\x84";
	dict[29] = "fast kovaa";
	dict[30] = "great iso";
	dict[31] = "grain jyv\x84";
	dict[32] = "god jumala";
	dict[33] = "gun ase";
	dict[34] = "gust puuska";
	dict[35] = "haste kiire";
	dict[36] = "hate viha";
	dict[37] = "honey hunaja";
	dict[38] = "horse pukki";
	dict[39] = "harm paha";
	dict[40] = "i min\x84";
	dict[41] = "ice j\x84\x84";
	dict[42] = "island saari";
	dict[43] = "impact isku";
	dict[44] = "it se";
	dict[45] = "joy ilo";
	dict[46] = "just vain";
	dict[47] = "jam hillo";
	dict[48] = "jump hyp\x84t\x84";
	dict[49] = "justice laki";
	dict[50] = "kind kiltti";
	dict[51] = "know tuntea";
	dict[52] = "knife veitsi";
	dict[53] = "knee polvi";
	dict[54] = "key avain";
	dict[55] = "lack puute";
	dict[56] = "luck onni";
	dict[57] = "lay panna";
	dict[58] = "love rakas";
	dict[59] = "lust himo";
	dict[60] = "mom \x84iti";
	dict[61] = "make tehd\x84";
	dict[62] = "more en\x84\x84";
	dict[63] = "man mies";
	dict[64] = "mere pelkk\x84";
	dict[65] = "night y\x94";
	dict[66] = "no ei";
	dict[67] = "name nimi";
	dict[68] = "nature luonto";
	dict[69] = "now nyt";
	dict[70] = "open avata";
	dict[71] = "own oma";
	dict[72] = "over yli";
	dict[73] = "order tilata";
	dict[74] = "obtain saada";
	dict[75] = "power teho";
	dict[76] = "pin tappi";
	dict[77] = "play pelata";
	dict[78] = "pay maksaa";
	dict[79] = "pace vauhti";
	dict[80] = "queen kuningatar";
	dict[81] = "queue jono";
	dict[82] = "quiet tyyni";
	dict[83] = "quit lakata";
	dict[84] = "quality laatu";
	dict[85] = "race rotu";
	dict[86] = "rush kiire";
	dict[87] = "run ajaa";
	dict[88] = "right oikeus";
	dict[89] = "rude tyly";
	dict[90] = "sad ik\x84v\x84";
	dict[91] = "say sanoa";
	dict[92] = "sake vuoksi";
	dict[93] = "sun aurinko";
	dict[94] = "sore kova";
	dict[95] = "take ottaa";
	dict[96] = "trust usko";
	dict[97] = "thought ajatus";
	dict[98] = "tongue kieli";
	dict[99] = "tell kertoa";
	dict[100] = "use k\x84ytt\x94";
	dict[101] = "under alla";
	dict[102] = "up yl\x94s";
	dict[103] = "until kunnes";
	dict[104] = "utilize k\x84ytt\x84\x84";
	dict[105] = "vane siipi";
	dict[106] = "value arvo";
	dict[107] = "view kanta";
	dict[108] = "valid p\x84tev\x84";
	dict[109] = "vacation loma";
	dict[110] = "wash pest\x84";
	dict[111] = "well hyvin";
	dict[112] = "waste hukata";
	dict[113] = "we me";
	dict[114] = "weed ruoho";
	dict[115] = "work ty\x94";
	dict[116] = "zoo el\x84 aintarha";
	dict[117] = "yoga jooga";
	dict[118] = "yes joo";
	dict[119] = "xylophone ksylofoni";
	dict[120] = "game peli";
	dict[121] = "toss nakata";
	dict[122] = "rich rikas";
	dict[123] = "poor huono";
	dict[124] = "rock kivi";
	const char *hello = "Welcome to DictOS!           ";
	pos = 0;
	caps_lock = 0;
	color = 0x7;
	unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
	for (int i = 0; i < 26; i++)
	{
		loaded_letters[i] = video_buf[0];
		video_buf += 2;
	}
	out_str(color, hello, str_pos++, pos);
	out_str(color, (char *)loaded_letters, str_pos++, pos);
	str_pos++;
	dict_sort();

	ticks = 0;
	cursor_moveto(str_pos, pos);
	intr_init();
	keyb_ticks_init();
	intr_start();
	intr_enable();
	load_dict();
	loop();
	return 0;
}
