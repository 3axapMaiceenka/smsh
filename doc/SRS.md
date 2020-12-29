# Требования к проекту
## Содержание
1. [Введение](#1) 
2. [Требования пользователя](#2) 
  2.1. [Программные интерфейсы](#2.1) 
  2.2. [Интерфейс пользователя](#2.2) 
  2.3. [Характеристики пользователей](#2.3) 
  2.4. [Предположения и зависимости](#2.4) 
3. [Системные требования](#3.) 
  3.1. [Функциональные требования](#3.1) 
  3.2. [Нефункциональные требования](#3.2)
     3.2.1. [Атрибуты качества](#3.2.1) 
## 1. Введение <a name="1"></a>
Данный проект направлен на разработку командной оболочки под названием "smsh" для операционных систем семейства Linux. "smsh" является интерпретатором команд пользователя, предоставляющим интерфейс для взаимодействия с операционной системой, и поддерживает простой самостоятельный язык программирования с собственным синтаксисом и отличительными функциональными возможностями.
## 2. Требования пользователя <a name="2"></a>
### 2.1 Программные интерфейсы <a name="2.1"></a>
Разработка будет вестись с использованием языка программирования C. Будут задействованы как стандартные библиотеки, так и сторонние, например, GNU Readline. В таблице 1 приведены сведения об стороннем программном обеспечении, которое будет использоваться в процессе разработки.

Таблица 1 - Используемое в разработке стороннее ПО
<table align="center">
  <tr>
    <td>Название</td>
    <td>C</td>
    <td>GNU Make</td>
    <td>gcc</td>
    <td>GNU Readline</td>
  </tr>
    <tr>
    <td>Описание</td>
    <td>Язык программирования</td>
    <td>Система сборки</td>
    <td>Компилятор</td>
    <td>Библиотека</td>
  </tr>
    <tr>
    <td>Версия</td>
    <td>C11</td>
    <td>3.75</td>
    <td>8.4</td>
    <td>8.1</td>
  </tr>
</table>
 
### 2.2 Интерфейс пользователя <a name="2.2"></a>
Взаимодействие с пользователем осуществляется посредством классического текстового интерфейса. Разрабатываемая командная оболочка способна выполнять команды пользователя как считывая их из стандартного потока ввода, так и при прочтении файла. 
### 2.3 Характеристики пользователя <a name="2.3"></a>
Программный продукт нацелен на рядового пользователя операционной системы семейства Linux, обладающего стандартными навыками. Пользователь должен освоить синтаксис языка интерпретатора команд.
### 2.4 Предположения и зависимости <a name="2.4"></a>
На перечисленные в данном документе требования к создаваемому программному продукту могут влиять сроки, отведенные для разработки. Необходимо наличие операционной системы семейства Linux.
## 3. Системные требования <a name="3"></a>
### 3.1 Функциональные требования <a name="3.1"></a>
Командная оболочка должна поддерживать два режима работы: интерактивный и неинтерактивный. В интерактивном режиме работы оболочка должна считывать ввод пользователя и выполнять заданные команды. В неинтерактивном режиме оболочка должна считать и выполнить команды из файла, имя которого передается в качестве аргумента командной строки.
В обоих режимах работы командный интерпретатор должен предоставлять пользователю следующие функции:
* <b>  Запуск исполняемых файлов. </b> Пользователю должна быть предоставлена  возможность начать выполнение заданного исполняемого файла, при этом командная оболочка должна осуществлять поиск исполняемых файлов, создание процессов и передачу им аргументов. Процессы могут быть запущены в фоновом режиме.  
* <b> Перенаправление стандартных потоков ввода и вывода. </b> Пользователю должна быть предоставлена  возможность перенаправлять стандартный поток ввода и/или вывода создаваемого процесса в файл.
* <b> Создание каналов. </b> Пользователю должна быть предоставлена  возможность создавать неограниченное число процессов таким образом, что стандартный поток вывода каждого процесса соединен через канал со стандартным процессов ввода последующего процесса.
* <b> Контроль задач (job control). </b> Пользователю должна быть предоставлена  возможность манипулировать группой процессов (а также одиночными процессами), соединенных между собой через каналы, одновременно: приостанавливать выполнение процессов, завершать выполнение процессов посредством отправки сигнала, возобновлять выполнение процессов, переводить группу процессов в фоновый режим или в обычный режим выполнения.
* <b> Встроенные команды </b>. Командная оболочка должна предоставить пользователю реализацию некоторых встроенных команд, которые влияют непосредственно на состояние оболочки и не могут быть реализованы в виде сторонних исполняемых файлов.
* <b> Переменные. </b> Пользователь должен иметь возможность создавать переменные, присваивать переменным значения строковых литералов, других переменных и результатов вычислений арифметических выражений.   
* <b>  Арифметические выражения. </b> Командная оболочка должна поддерживать вычисление следующих арифметических операций: сложение, вычитание, умножение, деление. Результат арифметической операций должен быть представлен в виде целого числа. В арифметических операциях могут быть использованы переменные.
* <b> Составные команды </b>. Командная оболочка должна поддерживать выполнение стандартных конструкций ветвления: цикл while, цикл for, конструкция if/else.
* <b> Обработка ошибок. </b> Командная оболочка должна адекватно реагировать на различные ненормальные ситуации, такие как несоблюдение пользователем синтаксиса языка командного интерпретатора или отсутствие заданного пользователем исполняемого файла.  
### 3.2 Нефункциональные требования <a name="3.2"></a>
#### 3.2.1 Атрибуты качества <a name="3.2.1"></a>
* Производительность. Быстрый парсинг и выполнение сложных команд.
*  Адекватная реакция на ошибки.