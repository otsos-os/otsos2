/* Калькулятор для среды Linux x86_64 Freestanding
   Полностью без заголовочных файлов и libc */

/* Вручную объявляем типы, чтобы не зависеть от stdint.h/stddef.h */
typedef long long          int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long      size_t;

/* Константы системных вызовов Linux x86_64 */
#define SYS_read  0
#define SYS_write 1
#define SYS_exit  60

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define NULL          ((void*)0)

/* Прямой системный вызов ядра через встроенный ассемблер */
static inline int64_t syscall3(int64_t number, int64_t arg1, int64_t arg2, int64_t arg3) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* Настоящая точка входа для линкера */
void _start(void) {
    char buffer[64];
    
    // Выводим приглашение к вводу
    const char msg[] = "Enter expression (e.g. 12+5): ";
    syscall3(SYS_write, STDOUT_FILENO, (int64_t)msg, sizeof(msg) - 1);

    // Читаем строку из терминала
    int64_t bytes_read = syscall3(SYS_read, STDIN_FILENO, (int64_t)buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        syscall3(SYS_exit, 0, 0, 0);
    }
    buffer[bytes_read] = '\0';

    int64_t a = 0, b = 0;
    char op = 0;
    int i = 0;

    // Парсим первое число (пропускаем пробелы/табуляцию)
    while (buffer[i] == ' ' || buffer[i] == '\t') i++;
    while (buffer[i] >= '0' && buffer[i] <= '9') {
        a = a * 10 + (buffer[i] - '0');
        i++;
    }

    // Ищем знак операции
    while (buffer[i] == ' ' || buffer[i] == '\t') i++;
    if (buffer[i] == '+' || buffer[i] == '-' || buffer[i] == '*' || buffer[i] == '/') {
        op = buffer[i];
        i++;
    }

    // Парсим второе число
    while (buffer[i] == ' ' || buffer[i] == '\t') i++;
    while (buffer[i] >= '0' && buffer[i] <= '9') {
        b = b * 10 + (buffer[i] - '0');
        i++;
    }

    int64_t result = 0;
    int valid = 1;

    // Вычисления
    if (op == '+')      result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b != 0) result = a / b;
        else valid = 0; // Защита от деления на ноль
    } else {
        valid = 0; // Неизвестный оператор
    }

    // Вывод результата без sprintf
    if (valid) {
        char out_buf[32];
        int out_idx = 30;
        out_buf[31] = '\n';
        
        int64_t temp = result;
        int is_negative = 0;
        
        if (temp < 0) {
            is_negative = 1;
            temp = -temp;
        }
        
        if (temp == 0) {
            out_buf[out_idx--] = '0';
        } else {
            while (temp > 0) {
                out_buf[out_idx--] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        
        if (is_negative) {
            out_buf[out_idx--] = '-';
        }

        const char res_label[] = "Result: ";
        syscall3(SYS_write, STDOUT_FILENO, (int64_t)res_label, sizeof(res_label) - 1);
        syscall3(SYS_write, STDOUT_FILENO, (int64_t)&out_buf[out_idx + 1], 31 - out_idx);
    } else {
        const char err_msg[] = "Error\n";
        syscall3(SYS_write, STDOUT_FILENO, (int64_t)err_msg, sizeof(err_msg) - 1);
    }

    // Выход из программы (SYS_exit)
    syscall3(SYS_exit, 0, 0, 0);
}