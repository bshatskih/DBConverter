
# DBConverter
 
Утилита командной строки для конвертации CSV и JSON файлов в SQLite базу данных. Автоматически определяет типы колонок, поддерживает вложенные JSON-структуры с созданием дочерних таблиц.
 
---
 
## Быстрый старт
 
### Требования
 
- CMake 3.14+
- Компилятор с поддержкой C++20 (GCC 11+, Clang 13+, MSVC 2022+)
- Python 3 (опционально - для запуска тестов через скрипт)
 
### Сборка
 
```bash
git clone https://github.com/your/DBConverter.git
cd DBConverter
cmake -B build
cmake --build build
```
 
Исполняемый файл появится в `build/bin/DBConverter`.
 
### Использование
 
```bash
# CSV -> SQLite
./DBConverter data.csv
 
# JSON -> SQLite
./DBConverter data.json
 
# Указать путь к выходному файлу явно
./DBConverter data.csv output.db
./DBConverter data.json output.db
```
 
Если выходной файл не указан - создаётся рядом с входным с тем же именем и расширением `.db`.
 
### Примеры
 
**CSV:**
```bash
./DBConverter examples/sample.csv
# -> examples/sample.db
# Таблица "sample" с автоматически определёнными типами колонок
```
 
**JSON с вложенными структурами:**
```bash
./DBConverter examples/sample.json
# -> examples/sample.db
# Таблица "sample" + дочерние таблицы для вложенных массивов объектов
```
 
### Запуск тестов
 
```bash
cmake --build build
python scripts/run_tests.py
```
 
---
 
 
 
## Содержание

## Содержание

1. [Процесс выполнения программы](#процесс-выполнения-программы)
   - [CSV to SQLite](#csv-to-sqlite)
   - [JSON to SQLite](#json-to-sqlite)
2. [parsers](#parsers)
   - [csv_parser](#parserscsv_parser)
   - [json_parser](#parsersjson_parser)
3. [models](#models)
   - [table_schema](#modelstable_schema)
   - [data_row](#modelsdata_row)
4. [utils](#utils)
   - [type_converter](#utilstype_converter)
   - [file_validator](#utilsfile_validator)
   - [string_utils](#utilsstring_utils)
5. [database](#database)
   - [db_manager](#databasedb_manager)
6. [Система сборки](#система-сборки)




# Процесс выполнения программы

## CSV to SQLite

#### 1. Запуск и валидация входных данных

Программа получает путь к CSV-файлу. Первым делом `csv_parser::parse` передаёт путь в `file_validator::validate` - до открытия файла нужно убедиться что он вообще пригоден для работы. Валидатор последовательно проверяет что путь не пустой, файл существует, является обычным файлом, доступен для чтения, не пустой, имеет расширение `.csv` или `.json`, и не является бинарным. Если хотя бы одна проверка не прошла - парсинг не начинается, бросается `csv_parse_exception` с текстом ошибки от валидатора.


#### 2. Чтение и нормализация заголовка

После успешной валидации файл открывается и читается первая строка - заголовок. Перед обработкой с неё срезается UTF-8 BOM если он есть (три байта `EF BB BF` в начале), и убирается `\r` если файл в Windows-формате.

Заголовок разбивается на поля через `parse_line` - ручной парсер с поддержкой кавычек. Затем каждое поле нормализуется через `to_sql_identifier` - пробелы и дефисы заменяются на `_`, спецсимволы отбрасываются, результат приводится к нижнему регистру. Дублирующиеся имена разрешаются через `make_unique_identifier` - добавляется числовой суффикс `_1`, `_2` и так далее.


#### 3. Чтение строк данных

Оставшиеся строки файла читаются в цикле. Пустые строки пропускаются. Каждая строка разбивается через тот же `parse_line`. Если количество полей не совпадает с количеством заголовков - бросается `csv_parse_exception` с номером строки и деталями расхождения. Все строки накапливаются в памяти в виде двумерного массива строк `raw_rows`.



#### 4. Определение типов колонок

По собранным данным строится схема таблицы. Для каждой колонки собираются все её значения из `raw_rows` и передаются в `type_converter::infer_column_type`. Тот проходит по каждому значению и определяет его тип через `infer_type` - проверки идут в порядке убывания специфичности: `Null` $\rightarrow$ `Integer` $\rightarrow$ `Real` $\rightarrow$ `Boolean` $\rightarrow$ числа с буквенным суффиксом (`"4.2u"`) $\rightarrow$ `Text`. Затем через `promote` тип колонки повышается до наименее специфичного из всех встреченных значений - например если в колонке есть и целые и вещественные числа, колонка получает тип `Real`. Итоговый `sql_type` маппится в `Sqlite_type` и вместе с именем колонки сохраняется в `table_schema`.


###### 5. Построение строк данных

После того как схема готова, для каждой строки `raw_rows` строится `data_row`. Каждое значение сначала обрезается через `trim`. Затем проверяется: если значение null-like (`""`, `"null"`, `"n/a"` и т.д.) - колонке выставляется `NULL`. Если булевое (`"true"`, `"yes"`, `"on"` и т.д.) - конвертируется в `"1"` или `"0"` чтобы корректно лечь в INTEGER-колонку. Иначе - сохраняется как есть. На выходе `csv_parser::parse` возвращает `csv_parse_result` со схемой и вектором строк.


#### 6. Открытие базы данных

`main.cpp` получает результат парсинга и передаёт путь к выходному файлу в конструктор `db_manager`. Тот вызывает `sqlite3_open` - если файл не существует, SQLite создаёт его; если существует, открывает. `db_manager` является владельцем соединения через RAII - при выходе из области видимости деструктор автоматически вызовет `sqlite3_close`.


#### 7. Создание таблицы

`db_manager::create_table` получает схему и в два шага создаёт таблицу. Сначала выполняется `DROP TABLE IF EXISTS` - если таблица с таким именем уже есть в базе, она удаляется вместе с данными. Затем выполняется `CREATE TABLE` с колонками из схемы. PK-колонка объявляется как `INTEGER PRIMARY KEY AUTOINCREMENT` если ключ генерируется автоматически, или `INTEGER PRIMARY KEY` если берётся из данных. Если таблица дочерняя, добавляется FK-колонка с `REFERENCES` на родительскую таблицу. Все имена оборачиваются в двойные кавычки через `quote_sql_identifier` - защита от зарезервированных слов SQLite.


#### 8. Вставка данных

`db_manager::insert_rows` открывает транзакцию и вставляет строки по одной через `insert_row`. Для каждой строки генерируется SQL вида `INSERT INTO "table" ("col1", ...) VALUES (?, ...)`, подготавливается через `sqlite3_prepare_v2`, и каждое значение биндится через `sqlite3_bind_*`. Перед биндингом сырая строка из `data_row` конвертируется в типизированный `sql_value` через `type_converter::convert` — с учётом типа колонки из схемы. Statement финализируется через RAII-guard в любом случае — даже при исключении. Если все строки вставлены успешно — транзакция фиксируется через `COMMIT`. При любой ошибке — `ROLLBACK` и исключение пробрасывается выше.


#### 9. Результат

На диске появляется `.db` файл с таблицей, структура которой отражает заголовки CSV, а типы колонок определены автоматически по содержимому данных.

---


## JSON to SQLite

#### 1. Запуск и валидация входных данных

Программа получает путь к JSON-файлу. Первым делом `json_parser::parse` передаёт путь в `file_validator::validate` - до открытия файла нужно убедиться что он пригоден для работы. Валидатор последовательно проверяет что путь не пустой, файл существует, является обычным файлом, доступен для чтения, не пустой, имеет расширение `.csv` или `.json`, и не является бинарным. Если хотя бы одна проверка не прошла - парсинг не начинается, бросается `json_parse_exception` с текстом ошибки от валидатора.

#### 2. Парсинг JSON

После успешной валидации файл открывается и передаётся в `nlohmann::json::parse`. Если файл содержит невалидный JSON - бросается `json_parse_exception` с описанием синтаксической ошибки. Затем проверяется тип корня: если примитив (`"hello"`, `42`, `true`) - бросается исключение, так как примитив не является табличными данными. Если корень объект - он оборачивается в массив из одного элемента, чтобы дальнейшая обработка была одинаковой для обоих случаев. Имя таблицы берётся из имени файла через `to_sql_identifier`.

#### 3. Обход массива объектов

Каждый элемент корневого массива передаётся в `process_object`. Тот обходит все поля объекта и принимает решение по типу значения: скаляры (`null`, `bool`, `integer`, `float`, `string`) записываются в `data_row` напрямую, вложенный объект разворачивается в плоские поля через `flatten_object`, массив объектов накапливается в `child_arrays` для последующей рекурсии, массив примитивов сериализуется в строку через запятую. Параллельно накапливается общий список заголовков `headers` - если у разных объектов разные наборы полей, отсутствующие поля вставляются как `NULL`.

#### 4. Определение типов колонок

После того как все строки собраны, для каждой колонки собираются все её значения и передаются в `type_converter::infer_column_type`. Логика та же что в CSV-парсере: проверки идут в порядке `Null` $\rightarrow$ `Integer` $\rightarrow$ `Real` $\rightarrow$ `Boolean` $\rightarrow$ числа с буквенным суффиксом $\rightarrow$ `Text`, тип колонки повышается через `promote` до наименее специфичного из всех встреченных значений.

#### 5. Построение схемы

Среди заголовков ищется колонка `id` - если найдена, она становится PK с `has_custom_pk = true` и убирается из списка обычных колонок. Если нет - PK будет `AUTOINCREMENT`. Если таблица дочерняя, схема строится с FK-колонкой вида `parent_table_id` и ссылкой `REFERENCES` на родительскую таблицу. Булевые значения нормализуются в `"1"`/`"0"`.

#### 6. Рекурсия для дочерних таблиц

После построения схемы для текущей таблицы `process_array` рекурсивно вызывает себя для каждого накопленного `child_arrays[field_name]`. Имя дочерней таблицы строится как `parent_table_field_name`. Каждый элемент дочернего массива хранит `parent_index` - порядковый номер родительской строки в её массиве. Таблицы добавляются в `result` в порядке родитель  дочерние, что гарантирует корректный порядок создания таблиц в SQLite.

#### 7. Открытие базы данных

`main.cpp` получает результат парсинга и передаёт путь к выходному файлу в конструктор `db_manager`. Тот вызывает `sqlite3_open` - если файл не существует, SQLite создаёт его; если существует, открывает. `db_manager` является владельцем соединения через RAII - при выходе из области видимости деструктор автоматически вызовет `sqlite3_close`.

#### 8. Создание таблиц

`main.cpp` итерируется по всем таблицам и для каждой вызывает `db_manager::create_table`. Сначала выполняется `DROP TABLE IF EXISTS`, затем `CREATE TABLE`. Порядок создания соответствует порядку в `json_parse_result` - родитель всегда раньше дочерних, иначе FK-constraint упадёт при создании дочерней таблицы.

#### 9. Вставка данных

Таблицы вставляются по одной в том же порядке. Для каждой таблицы открывается транзакция. Если таблица дочерняя - перед вставкой каждой строки берётся `parent_index` из `parent_indices[i]`, по нему из карты `rowids` достаётся реальный `rowid` родительской строки, и он проставляется в FK-колонку. После вставки строки её `rowid` сохраняется через `last_insert_rowid` - он понадобится дочерним таблицам следующего уровня. При успехе транзакция фиксируется через `COMMIT`, при ошибке - `ROLLBACK`.

#### 10. Результат

На диске появляется `.db` файл с одной или несколькими таблицами. Корневой массив становится главной таблицей, вложенные массивы объектов становятся дочерними таблицами связанными через FK. Типы колонок определены автоматически по содержимому данных.


---


# parsers


## parsers::csv_parser

### Назначение

`csv_parser` — stateless-класс (все методы статические, конструктор удалён) для парсинга CSV-файлов в структуры готовые к записи в SQLite.

На выходе даёт `table_schema` с автоматически определёнными типами колонок и вектор `data_row` с нормализованными значениями.

Класс находится в пространстве имён `parsers`.

---

### Расположение

```
src/
└── parsers/
    ├── csv_parser.h
    └── csv_parser.cpp
```

---

### Типы данных

- **`csv_parse_exception`** - исключение для всех ошибок парсинга. Наследуется от `std::runtime_error`. Бросается при любой ошибке парсинга - провале валидации файла, некорректном формате строки, несовпадении количества полей. Содержит человекочитаемое сообщение с деталями ошибки.

   ```cpp
   class csv_parse_exception : public std::runtime_error {
   public:
      explicit csv_parse_exception(const std::string& message);
   };
   ```

- **`csv_parse_result`** - результат парсинга CSV-файла. Содержит `table_schema` с определёнными типами колонок и вектор `data_row` с нормализованными строками данных. Не имеет дефолтного конструктора из-за наличия `table_schema`, поэтому переменные этого типа должны быть инициализированы при объявлении.

   ```cpp
   struct csv_parse_result {
      models::table_schema          schema;
      std::vector rows;
   };
   ```

---




### Интерфейс

- **`parse`** - основной публичный метод класса. Читает CSV-файл по указанному пути, валидирует его, парсит строки, определяет типы колонок и возвращает `csv_parse_result`. Бросает `csv_parse_exception` при любой ошибке.

   ```cpp
   [[nodiscard]] static csv_parse_result parse(const std::filesystem::path& path);
   ```

   **Гарантии:**
   - Первая строка файла всегда трактуется как заголовок
   - Разделитель - только запятая
   - Каждая строка данных содержит ровно столько полей сколько заголовков — иначе `csv_parse_exception`
   - Пустые строки в файле пропускаются
   - Имена колонок нормализуются в валидные SQL-идентификаторы через `to_sql_identifier`
   - Дубликаты заголовков разрешаются добавлением числового суффикса (`col`, `col_1`, `col_2`)

   **Бросает `csv_parse_exception` если:**
   - Файл не прошёл валидацию (`file_validator`)
   - Файл не содержит заголовка
   - Строка данных имеет неверное количество полей
   - Встречено незакрытое кавычное поле

---

### Внутренние методы

- **`parse_line`** - разбивает одну строку CSV на поля с учётом кавычек по стандарту RFC 4180. Бросает `csv_parse_exception` при обнаружении незакрытой кавычки. Используется внутри `parse` для обработки каждой строки данных.

   ```cpp
   [[nodiscard]] static std::vector parse_line(std::string_view line);
   ```

   | Случай | Пример | Результат |
   |--------|--------|-----------|
   | Обычное поле | `hello` | `hello` |
   | Кавычное поле | `"hello, world"` | `hello, world` |
   | Удвоенная кавычка | `""O'Brien""` | `"O'Brien"` |
   | Пустое поле | `a,,b` | `[a, "", b]` |
   | Незакрытая кавычка | `"hello` | `csv_parse_exception` |



- **`build_schema`** - строит `table_schema` по заголовкам и всем строкам данных. Для каждой колонки собирает все её значения и вызывает `type_converter::infer_column_type`. Бросает `csv_parse_exception` если тип колонки не может быть определён (например, смешанные типы без явного преобладания).

   ```cpp
   [[nodiscard]] static models::table_schema build_schema(
      const std::string& table_name,
      const std::vector& headers,
      const std::vector<std::vector>& raw_rows);
   ```

   Маппинг `sql_type` $\Rightarrow$ `Sqlite_type`:

   | `sql_type` | `Sqlite_type` | Примечание |
   |------------|---------------|------------|
   | `Integer`  | `INTEGER`     |            |
   | `Boolean`  | `INTEGER`     | `"true"`/`"yes"` конвертируются в `"1"`/`"0"` при построении `data_row` |
   | `Real`     | `REAL`        |            |
   | `Null`     | `TEXT`        | Все значения null-like $\Rightarrow$ безопасный дефолт |
   | `Text`     | `TEXT`        |            |

---

### Зависимости

| Компонент        | Роль                                                                      |
|------------------|---------------------------------------------------------------------------|
| `file_validator` | Валидация файла перед парсингом — существование, размер, расширение, бинарность |
| `string_utils`   | `trim`, `is_blank`, `is_null_like`, `is_boolean`, `to_sql_identifier`, `make_unique_identifier` |
| `type_converter` | `infer_column_type` для определения типов колонок, `convert` для булевых значений |
| `table_schema`   | Хранит имя таблицы и список колонок с типами                              |
| `data_row`       | Хранит значения одной строки — `optional<string>` на каждую колонку       |

---



## parsers::json_parser


### Назначение

`json_parser` - stateless-класс (все методы статические, конструктор удалён) для парсинга JSON-файлов в структуры готовые к записи в SQLite. В отличие от `csv_parser`, один JSON-файл может породить несколько таблиц - корневую и произвольное количество дочерних для вложенных массивов объектов.

Класс находится в пространстве имён `parsers`.

---

### Расположение

```
src/
└── parsers/
    ├── json_parser.h
    └── json_parser.cpp
```

---



### Типы данных
 
- **`json_parse_exception`**
   ```cpp
   class json_parse_exception : public std::runtime_error {
   public:
      explicit json_parse_exception(const std::string& message);
   };
   ```
   Бросается при любой ошибке - провале валидации файла, синтаксической ошибке JSON, или неожиданном примитиве на месте ожидаемого объекта.
 

 
- **`child_entry`**
   ```cpp
   struct child_entry {
      int64_t parent_index;        // индекс родительской строки в её таблице
      nlohmann::json element;      // сам JSON-объект
   };
   ```
   
   Внутренняя структура для передачи элементов дочерних массивов между методами. `parent_index` - это позиция родительской строки в её массиве (0, 1, 2...), не реальный `rowid` в базе. Реальный `rowid` подставляется при вставке в `main.cpp`.
 
- **`table_with_rows`**
   ```cpp
   struct table_with_rows {
      models::table_schema schema;
      std::vector<models::data_row> rows;
      std::vector<int64_t> parent_indices;
   };
   ```
   
   Одна таблица с данными. `parent_indices` - вектор индексов родительских строк, по одному на каждую строку. Для корневой таблицы вектор пустой.
   
   Пример для трёх пользователей с заказами:
   
   ```
   users.parent_indices = []          // корневая - пустой
   orders.parent_indices = [0, 0, 1]  // первые два заказа принадлежат users[0],
                                      // третий — users[1]
   ```
 
- **`json_parse_result`**
   
   ```cpp
   struct json_parse_result {
      std::vector<table_with_rows> tables;
   };
   ```
   
   Результат парсинга - все таблицы в порядке родитель $\Rightarrow$ дочерние. Этот порядок критичен для вставки в SQLite: родительская таблица должна существовать до вставки дочерних строк с FK.
   
---
 

### Интерфейс

- **`parse`** - основной публичный метод класса. Читает JSON-файл по указанному пути, валидирует его, парсит рекурсивно, строит схемы и строки для всех таблиц, и возвращает `json_parse_result`. Бросает `json_parse_exception` при любой ошибке.
   
   ```cpp
   [[nodiscard]] static json_parse_result parse(const std::filesystem::path& path);
   ```
   
   **Гарантии:**
   - Корень массив $\rightarrow$ каждый элемент это строка корневой таблицы
      ```json
      [
         { "id": 1, "name": "Alice" },
         { "id": 2, "name": "Bob"   }
      ]
      ```
   - Корень объект $\rightarrow$ оборачивается в массив из одного элемента
      ```json
      {
         "id": 1,
         "name": "Alice",
         "orders": [...]
      }
      ```
   - Корень примитив $\rightarrow$ `json_parse_exception`
      ```json
      "hello" / 42 / true / null
      ```
   - Имя таблицы берётся из имени файла через `to_sql_identifier`
   - Имена колонок нормализуются через `to_sql_identifier`
   - Таблицы в результате упорядочены: родитель раньше дочерних
   
   **Бросает `json_parse_exception` если:**
   - Файл не прошёл валидацию (`file_validator`)
   - Файл содержит невалидный JSON
   - Корень JSON — примитив (`"hello"`, `42`, `true`)
   - Элемент массива не является объектом
   
   ```cpp
   try {
      auto result = parsers::json_parser::parse("data.json");
      for (const auto& table : result.tables) {
         std::cout << table.schema.name() << ": "
                     << table.rows.size() << " rows\n";
      }
   } catch (const parsers::json_parse_exception& e) {
      std::cerr << e.what() << "\n";
   }
   ```
 
---



### Внутренние методы
 
- **`process_array`** 
   ```cpp
   static void process_array(
      const std::string& table_name,
      const std::string& parent_table,
      const std::vector<child_entry>& entries,
      std::vector<table_with_rows>& result);
   ```
   
   Обрабатывает массив объектов - строит одну таблицу и рекурсивно запускает себя для всех вложенных массивов объектов. Принимает уже собранные `child_entry` - элементы с их `parent_index`.
   
   Порядок работы:
      1. Для каждого элемента вызывает `process_object` $\rightarrow$ получает `data_row` и накапливает дочерние массивы в `child_arrays`
      2. По всем собранным строкам определяет типы колонок через `infer_columns`
      3. Ищет колонку `id` - если есть, делает её PK
      4. Нормализует булевые значения в `"1"`/`"0"`
      5. Строит `table_schema` и добавляет таблицу в `result`
      6. Для каждого накопленного дочернего массива рекурсивно вызывает `process_array`
   

 
- **`process_object`** -  обрабатывает один JSON-объект и принимает решение что делать с каждым полем.
   
   ```cpp
   static models::data_row process_object(
      const std::string& table_name,
      const nlohmann::json& object,
      std::vector<std::string>& headers,
      int64_t row_index,
      std::unordered_map<std::string, std::vector<child_entry>>& child_arrays);
   ```
   
   Маппинг типов JSON, поведение:
   
   | Тип JSON | Поведение |
   |----------|-----------|
   | `null` | `set_null` $\rightarrow$ `NULL` в базе |
   | `bool` | `"1"` или `"0"` |
   | `integer` | `std::to_string` |
   | `float` | `snprintf("%.15g")` |
   | `string` | как есть |
   | `object` | `flatten_object` - разворачивается в плоские поля с префиксом |
   | `array` пустой | пропускается |
   | `array` объектов | накапливается в `child_arrays[field_name]` с текущим `row_index` |
   | `array` примитивов | сериализуется в строку через запятую |
   

- **`flatten_object`**
   
   ```cpp
   static void flatten_object(
      const std::string& prefix,
      const nlohmann::json& object,
      models::data_row& row,
      std::vector<std::string>& headers);
   ```
   
   Рекурсивно разворачивает вложенный объект в плоские колонки. Имя колонки строится как `prefix_key`. Поддерживает произвольную глубину вложенности объектов. Разворачивание в плоские колонки - когда встречаем вложенный объект, мы не создаём дочернюю таблицу, а просто добавляем его поля в текущую строку с префиксом:
   ```json
   {
   "id": 1,
   "address": {
      "city": "Moscow",
      "zip":  "101000"
      }
   }
   ```

   `flatten_object` вызывается с `prefix = "address"` и разворачивает объект в:
   ```json
   "address" + "_" + "city" -> колонка "address_city" = "Moscow"
   "address" + "_" + "zip"  -> колонка "address_zip"  = "101000"
   ```

   Произвольная глубина - если внутри объекта снова объект, рекурсия продолжается:
   
   Массивы объектов внутри `flatten_object` пропускаются - они должны обрабатываться через `process_object` как дочерние таблицы, а не как плоские поля.
   Пример:
   ```json
   {
      "id": 1,
      "meta": {
         "tags": [
            { "name": "vip" },
            { "name": "active" }
         ]
      }
   }
   ```
   Когда `flatten_object` доходит до `"tags"` - это массив объектов внутри вложенного объекта `meta`. Создать дочернюю таблицу здесь нельзя - `flatten_object` не имеет доступа к `child_arrays`, он только пишет в `data_row`. Поэтому `tags` просто пропускается.

 
- **`infer_columns`** - определяет типы колонок по всем строкам. Логика та же что в `csv_parser::build_schema` - двухпроходная обработка через `type_converter::infer_column_type`.
   ```cpp
   [[nodiscard]] static std::vector<models::Column> infer_columns(
      const std::vector<std::string>& headers,
      const std::vector<models::data_row>& rows);
   ```
 
- **`serialize_primitive_array`** - сериализует массив примитивов в строку через запятую. Например, массив `[1, "two", true]` будет сериализован в строку `"1,two,1"`. Используется для массивов примитивов внутри `process_object` - такие массивы не порождают дочернюю таблицу, а просто сохраняются в виде строки.
 
   ```cpp
   [[nodiscard]] static std::string serialize_primitive_array(const nlohmann::json& array);
   ```
   

- **`ensure_header`**
   ```cpp
   static void ensure_header(
      std::vector<std::string>& headers,
      const std::string& col_name);
   ```
   
   Добавляет имя колонки в список если его ещё нет. Используется для накопления уникального набора заголовков при обходе объектов с разными наборами полей.

---

### Поддерживаемые форматы JSON


#### Формат 1 - Массив плоских объектов
 
Простейший случай. Каждый объект в массиве становится строкой одной таблицы.
 
```json
[
  { "id": 1, "name": "Alice", "age": 30 },
  { "id": 2, "name": "Bob",   "age": 25 }
]
```
 
Результат - одна таблица `users`:
 
```sql
CREATE TABLE "users" (
  "id"   INTEGER PRIMARY KEY,
  "name" TEXT,
  "age"  INTEGER
);
```
 
#### Формат 2 - Массив объектов с вложенными массивами объектов
 
Вложенный массив объектов становится дочерней таблицей. FK проставляется автоматически.
 
```json
[
  {
    "id": 1,
    "name": "Alice",
    "orders": [
      { "id": 101, "amount": 50.0, "item": "book" },
      { "id": 102, "amount": 75.0, "item": "pen"  }
    ]
  },
  {
    "id": 2,
    "name": "Bob",
    "orders": [
      { "id": 201, "amount": 30.0, "item": "notebook" }
    ]
  }
]
```
 
Результат - две таблицы:
 
```sql
CREATE TABLE "users" (
  "id"   INTEGER PRIMARY KEY,
  "name" TEXT
);
 
CREATE TABLE "users_orders" (
  "id"      INTEGER PRIMARY KEY AUTOINCREMENT,
  "users_id" INTEGER NOT NULL REFERENCES "users",
  "id"      INTEGER,
  "amount"  REAL,
  "item"    TEXT
);
```
 
Данные в `users_orders`:
 
| users_id | id  | amount | item     |
|----------|-----|--------|----------|
| 1        | 101 | 50.0   | book     |
| 1        | 102 | 75.0   | pen      |
| 2        | 201 | 30.0   | notebook |

 
#### Формат 3 - Рекурсивная вложенность
 
Вложенность на любую глубину - каждый уровень массива объектов порождает свою таблицу. Имена таблиц строятся как цепочка `parent_child_grandchild`.
 
```json
[
  {
    "id": 1,
    "name": "Alice",
    "orders": [
      {
        "id": 101,
        "amount": 50.0,
        "items": [
          { "product": "book",   "qty": 2 },
          { "product": "pencil", "qty": 5 }
        ]
      }
    ]
  }
]
```
 
Результат - три таблицы:
 
```sql
CREATE TABLE "users" ( ... );
 
CREATE TABLE "users_orders" (
  "id"       INTEGER PRIMARY KEY AUTOINCREMENT,
  "users_id" INTEGER NOT NULL REFERENCES "users",
  ...
);
 
CREATE TABLE "users_orders_items" (
  "id"             INTEGER PRIMARY KEY AUTOINCREMENT,
  "users_orders_id" INTEGER NOT NULL REFERENCES "users_orders",
  "product"        TEXT,
  "qty"            INTEGER
);
```
 

 
#### Формат 4 - Вложенный объект (не массив)
 
Вложенный объект разворачивается в плоские колонки с префиксом. Дочерней таблицы не создаётся.
 
```json
[
  {
    "id": 1,
    "name": "Alice",
    "address": {
      "city":    "Moscow",
      "zip":     "101000",
      "country": "Russia"
    }
  }
]
```
 
Результат - одна таблица с плоской структурой:
 
```sql
CREATE TABLE "users" (
  "id"              INTEGER PRIMARY KEY,
  "name"            TEXT,
  "address_city"    TEXT,
  "address_zip"     TEXT,
  "address_country" TEXT
);
```
 
#### Формат 5 - Глубоко вложенные объекты
 
Flatten работает рекурсивно - объект внутри объекта тоже разворачивается.
 
```json
[
  {
    "id": 1,
    "location": {
      "city": "Moscow",
      "coords": {
        "lat": 55.75,
        "lon": 37.61
      }
    }
  }
]
```
 
Результат:
 
```sql
CREATE TABLE "data" (
  "id"                   INTEGER PRIMARY KEY,
  "location_city"        TEXT,
  "location_coords_lat"  REAL,
  "location_coords_lon"  REAL
);
```
 
 
#### Формат 6 - Массив примитивов внутри объекта
 
Массив примитивов сериализуется в строку через запятую и хранится как TEXT.
 
```json
[
  {
    "id":   1,
    "name": "Bulbasaur",
    "types": ["Grass", "Poison"],
    "moves": ["tackle", "growl", "vine-whip"]
  }
]
```
 
Результат:
 
```sql
CREATE TABLE "pokemon" (
  "id"    INTEGER PRIMARY KEY,
  "name"  TEXT,
  "types" TEXT,   -- "Grass,Poison"
  "moves" TEXT    -- "tackle,growl,vine-whip"
);
```
 
 
#### Формат 7 - Одиночный объект (не массив)
 
Корневой объект оборачивается в массив из одного элемента.
 
```json
{
  "id":   1,
  "name": "Alice",
  "age":  30
}
```
 
Результат - таблица из одной строки:
 
```sql
CREATE TABLE "data" (
  "id"   INTEGER PRIMARY KEY,
  "name" TEXT,
  "age"  INTEGER
);
```
 
 
#### Формат 8 - Объекты с разными наборами полей
 
Объекты в массиве не обязаны иметь одинаковые поля. Отсутствующие поля вставляются как `NULL`.
 
```json
[
  { "id": 1, "name": "Alice", "email": "alice@example.com" },
  { "id": 2, "name": "Bob" },
  { "id": 3, "name": "Charlie", "phone": "+7 999 000 00 00" }
]
```
 
Результат:
 
```sql
CREATE TABLE "users" (
  "id"    INTEGER PRIMARY KEY,
  "name"  TEXT,
  "email" TEXT,   -- NULL для Bob и Charlie
  "phone" TEXT    -- NULL для Alice и Bob
);
```
 

 
### Формат 9 - Комбинированный
 
Все возможности вместе: вложенные объекты (flatten), вложенные массивы объектов (дочерние таблицы), массивы примитивов (сериализация).
 
```json
[
  {
    "id": 1,
    "name": "Alice",
    "address": { "city": "Moscow", "zip": "101000" },
    "tags": ["vip", "active"],
    "orders": [
      {
        "id": 101,
        "amount": 150.0,
        "items": [
          { "product": "book", "qty": 2 }
        ]
      }
    ]
  }
]
```
 
Результат - три таблицы:
 
```sql
-- Таблица 1: корневая
CREATE TABLE "users" (
  "id"           INTEGER PRIMARY KEY,
  "name"         TEXT,
  "address_city" TEXT,     -- flatten объекта address
  "address_zip"  TEXT,     -- flatten объекта address
  "tags"         TEXT      -- "vip,active" — массив примитивов
);
 
-- Таблица 2: дочерняя
CREATE TABLE "users_orders" (
  "id"       INTEGER PRIMARY KEY AUTOINCREMENT,
  "users_id" INTEGER NOT NULL REFERENCES "users",
  "id"       INTEGER,
  "amount"   REAL
);
 
-- Таблица 3: дочерняя второго уровня
CREATE TABLE "users_orders_items" (
  "id"              INTEGER PRIMARY KEY AUTOINCREMENT,
  "users_orders_id" INTEGER NOT NULL REFERENCES "users_orders",
  "product"         TEXT,
  "qty"             INTEGER
);
```
 

---
 
### Неподдерживаемые форматы
 
### Корень — примитив
 
```json
"hello"
```
```json
42
```
 
Бросается `json_parse_exception`. JSON должен быть объектом или массивом.
 
---
 
#### Корень - объект объектов (словарь сущностей)
 
```json
{
  "bulbasaur":  { "hp": 45, "attack": 49 },
  "charmander": { "hp": 39, "attack": 52 }
}
```
 
Парсер обработает это как **один объект** с полями `bulbasaur_hp`, `bulbasaur_attack`, `charmander_hp`... - то есть одна строка с тысячами колонок. Это не ошибка парсера, но результат почти наверняка не тот что ожидается.
 
Нужно предварительно конвертировать в массив:
 
```python
import json
 
with open("input.json") as f:
    data = json.load(f)
 
result = [{"name": k, **v} for k, v in data.items()]
 
with open("output.json", "w") as f:
    json.dump(result, f)
```
 
После конвертации:
 
```json
[
  { "name": "bulbasaur",  "hp": 45, "attack": 49 },
  { "name": "charmander", "hp": 39, "attack": 52 }
]
```
 
 
#### Массив смешанных типов
 
```json
[
  { "id": 1, "name": "Alice" },
  "some string",
  42
]
```
 
Бросается `json_parse_exception` - все элементы корневого массива должны быть объектами.
 
 
#### Массив объектов внутри flatten
 
```json
[
  {
    "id": 1,
    "meta": {
      "tags": [
        { "name": "vip" },
        { "name": "active" }
      ]
    }
  }
]
```
 
Массив объектов внутри вложенного объекта (`meta.tags`) **пропускается** - `flatten_object` не создаёт дочерних таблиц. Чтобы `tags` стал дочерней таблицей, он должен находиться на верхнем уровне объекта, а не внутри вложенного объекта:
 
```json
[
  {
    "id": 1,
    "meta_description": "some text",
    "tags": [
      { "name": "vip" },
      { "name": "active" }
    ]
  }
]
```

---



















# database

## database::db_manager

### Назначение

`db_manager` — класс для работы с SQLite базой данных. Инкапсулирует жизненный цикл соединения, генерацию DDL по `table_schema`, и вставку строк через typed `sql_value` с использованием `sqlite3_bind_*`.

---
   
### Расположение

```
src/
└── database/
    ├── db_manager.h
    └── db_manager.cpp
```

---


### Исключение `db_exception`

```cpp
class db_exception : public std::runtime_error {
public:
    explicit db_exception(const std::string& message);
};
```

Бросается при любой ошибке SQLite — открытие базы, выполнение DDL, подготовка или выполнение statement. Содержит сообщение с текстом ошибки из `sqlite3_errmsg`.

---

### Конструктор и деструктор
   ```cpp
   explicit db_manager(const std::filesystem::path& path);
   ```

   Открывает (или создаёт) SQLite базу по указанному пути. При ошибке бросает `db_exception` до того как объект будет полностью сконструирован - утечки ресурсов нет.

   ```cpp
   ~db_manager();
   ```

   Закрывает соединение через `sqlite3_close`. Вызывается автоматически при выходе объекта из области видимости.

---

### Семантика владения

   `db_manager` некопируемый, но перемещаемый:
   ```cpp
   db_manager(const db_manager&) = delete;
   db_manager& operator=(const db_manager&) = delete;
   db_manager(db_manager&&) noexcept;
   db_manager& operator=(db_manager&&) noexcept;
   ```

---

### Управление транзакциями

   ```cpp
   void begin_transaction();
   void commit();
   void rollback();
   ```

   Явное управление транзакциями. При ошибке бросают `db_exception`.

   | Метод | Выполняемая команда |
   |-------|---------------------|
   | `begin_transaction()` | `BEGIN TRANSACTION` |
   | `commit()` | `COMMIT` |
   | `rollback()` | `ROLLBACK` |

   > **Причины необходимости транзакций при массовой вставке**
   >
   > SQLite по умолчанию оборачивает каждый `INSERT` в отдельную транзакцию с записью на диск. Для 1000 строк это 1000 fsync-операций. Явная транзакция сокращает это до одной - ускорение в десятки раз.

   ```cpp
   // Ручное управление транзакцией:
   db.begin_transaction();
   try {
      for (const auto& row : rows) {
         db.insert_row(schema, row);
      }
      db.commit();
   } catch (...) {
      db.rollback();
      throw;
   }
   ```
---

### DDL
   **`create_table`** - генерирует DDL для создания таблицы по переданной `table_schema` и выполняет его.

   ```cpp
   void create_table(const models::table_schema& schema);
   ```

   Сначала выполняет `DROP TABLE IF EXISTS`, затем `CREATE TABLE` по переданной схеме. Это гарантирует чистый результат при повторном запуске - существующая таблица с данными будет удалена.

   Правила генерации:
   - Все имена оборачиваются в двойные кавычки через `quote_sql_identifier` - защита от зарезервированных слов SQLite
   - PK: если `has_custom_pk = true` - `INTEGER PRIMARY KEY` (значение берётся из данных); если `false` - `INTEGER PRIMARY KEY AUTOINCREMENT`
   - FK: добавляется только если `schema.is_child() = true`; ссылается на родительскую таблицу через `REFERENCES`
   - Колонки из `schema.columns()` - не включают PK и FK, они хранятся отдельно в `table_schema`

### Вставка данных

- **`insert_row`**

   ```cpp
   void insert_row(const models::table_schema& schema, const models::data_row& row);
   ```

   Вставляет одну строку. Транзакция **не создаётся автоматически** - управление на вызывающем коде.

   Внутри:
      1. Генерирует `INSERT INTO "table" ("pk"(при наличии уникального ключа), "fk"(если есть родительская таблица), "col1", ...) VALUES (?, ?, ...)`
      2. Подготавливает statement через `sqlite3_prepare_v2`
      3. Биндит значения через `sqlite3_bind_*` в порядке: PK (если `has_custom_pk`) --> FK (если `is_child`) --> остальные колонки
      4. Выполняет через `sqlite3_step`
      5. Финализирует statement через RAII-guard - даже при исключении

   ```cpp
   models::data_row row;
   row.set("name", "Alice");
   row.set("age", "30");
   db.insert_row(schema, row);
   ```

- **`insert_rows`**
   ```cpp
   void insert_rows(const models::table_schema& schema, const std::vector& rows);
   ```

   Вставляет все строки в **одной транзакции**. При ошибке на любой строке делает `ROLLBACK` и пробрасывает исключение - база остаётся в консистентном состоянии.

   ```cpp
   db.insert_rows(schema, rows); // BEGIN --> INSERT × N --> COMMIT (или ROLLBACK)
   ```

   Внутри итерируется через range-based for, делегируя каждую строку в `insert_row`.

### Внутренние методы

Все методы приватные - не являются частью публичного интерфейса класса.


- **`execute`** - выполняет произвольный SQL-скрипт через `sqlite3_exec`. Используется для DDL и управления транзакциями. При ошибке бросает `db_exception` с текстом из `sqlite3_errmsg` и телом запроса.
   ```cpp
   void execute(const std::string& sql_script);
   ```

   Выполняет произвольный SQL через `sqlite3_exec`. Используется для DDL и управления транзакциями. При ошибке бросает `db_exception` с текстом из `sqlite3_errmsg` и телом запроса.

- **`bind_value`** - биндит `sql_value` в prepared statement. Маппинг альтернатив variant:

   ```cpp
   void bind_value(sqlite3_stmt* stmt, int index, const utils::sql_value& value);
   ```


   | Альтернатива  | SQLite функция          | Примечание                          |
   |---------------|-------------------------|-------------------------------------|
   | `nullptr_t`   | `sqlite3_bind_null`     |                                     |
   | `int64_t`     | `sqlite3_bind_int64`    |                                     |
   | `double`      | `sqlite3_bind_double`   |                                     |
   | `bool`        | `sqlite3_bind_int`      | `true` → 1, `false` → 0             |
   | `std::string` | `sqlite3_bind_text`     | `SQLITE_TRANSIENT` — SQLite копирует строку |

   > Альтернатива `SQLITE_STATIC` сообщила бы SQLite что строка живёт вечно.
   > Но `sql_value` — временный объект внутри `visit`, поэтому нужна копия.

- **`build_create_table_sql`** - генерирует DDL-строку для `CREATE TABLE` по переданной `table_schema`. Вынесен в отдельный метод для тестируемости - можно проверить что DDL соответствует ожиданиям для разных схем без необходимости взаимодействовать с реальной базой данных.

   ```cpp
   std::string build_create_table_sql(const models::table_schema& schema);
   ```
- **`build_insert_sql`** - генерирует шаблон INSERT с плейсхолдерами `?`. Порядок колонок: PK (если `has_custom_pk`) --> FK (если `is_child`) --> колонки из `schema.columns()`. Вызывается внутри каждого `insert_row` — statement не кешируется.
   ```cpp
   std::string build_insert_sql(const models::table_schema& schema);
   ```



   > При массовой вставке это означает повторную генерацию и компиляцию SQL на каждую строку.
   > Для больших файлов можно оптимизировать кешированием prepared statement - не реализовано.

---



### Зависимости

| Компонент        | Роль                                                                 |
|------------------|----------------------------------------------------------------------|
| `table_schema`   | Описывает структуру таблицы — имена колонок, типы, PK, FK            |
| `data_row`       | Хранит сырые строковые значения ячеек (`optional<string>`)           |
| `type_converter` | Конвертирует сырые строки в `sql_value` внутри `insert_row`          |
| `string_utils`   | Используется в `build_create_table_sql` для `quote_sql_identifier`   |

---












# models

## models::table_schema

### Назначение

Класс описывает структуру таблицы SQLite, построенную по данным из CSV или JSON.
Формируется парсером - один раз на файл для CSV, рекурсивно для каждого вложенного объекта в JSON. Не хранит сами данные, только схему. Типы колонок задаются через `type_converter`, который анализирует данные и определяет `Sqlite_type` для каждой колонки.

---
   
### Расположение

```
src/
└── models/
    ├── table_schema.h
    └── table_schema.cpp
```

---

### Типы данных

- **`Sqlite_type`** -  подмножество SQLite affinity типов достаточное для конвертации CSV/JSON. `BLOB` и `NUMERIC` не используются - бинарных данных в CSV/JSON не бывает, а `NUMERIC` это специфический SQLite affinity без практической пользы для данного проекта.
   ```cpp
   enum class Sqlite_type {
      INTEGER,
      REAL,
      TEXT
   };
   ```

   | Тип | Когда используется |
   |-----|--------------------|
   | `INTEGER` | целые числа, булевы значения (`true/false` -> `1/0`) |
   | `REAL` | вещественные числа |
   | `TEXT` | строки, массивы примитивов сериализованные через запятую |


- **`Column`** - простая структура: пара имя + тип. Логики не содержит. Имя хранится в нормализованном виде после `string_utils::to_sql_identifier`.

   ```cpp
   struct Column {
      std::string name;
      Sqlite_type type;
   };
   ```

   Пример использования:
   ```cpp
   Column col;
   col.name = "first_name";
   col.type = Sqlite_type::TEXT;
   ```

---

### Конструкторы

- **`Корневая таблица`**
   ```cpp
   table_schema(std::string name,
               std::vector columns,
               std::string pk_column = "id",
               bool has_custom_pk = false);
   ```
   Используется для CSV и верхнего уровня JSON.

   ```cpp
   // CSV: заголовки ["name", "age", "score"]
   table_schema schema(
      "users",
      {{"name", Sqlite_type::TEXT}, {"age", Sqlite_type::INTEGER}, {"score", Sqlite_type::REAL}},
      "id",
      false  // PK сгенерирован — AUTOINCREMENT
   );
   ```

   ```cpp
   // JSON: в данных есть поле "id"
   table_schema schema(
      "users",
      {{"name", Sqlite_type::TEXT}, {"age", Sqlite_type::INTEGER}},
      "id",
      true  // PK взят из данных
   );
   ```

---

- **`Дочерняя таблица`** - используется для вложенных объектов и массивов объектов в JSON.
   ```cpp
   table_schema(std::string name,
               std::vector columns,
               std::string pk_column,
               bool has_custom_pk,
               std::string parent_table,
               std::string foreign_key);
   ```

   ```cpp
   // JSON: {"id": 1, "orders": [{"id": 1, "amount": 99.9}]}
   table_schema orders_schema(
      "orders",
      {{"amount", Sqlite_type::REAL}},
      "id",
      true,
      "users",    // родительская таблица
      "users_id"  // FK колонка в таблице orders
   );
   ```

---


### Основные свойства

- **`name`** - возвращает имя таблицы в нормализованном виде.
   ```cpp
   [[nodiscard]] const std::string& name() const;
   ```

- **`columns`** - возвращает список колонок таблицы. Не включает PK и FK — они хранятся отдельно и обрабатываются `db_manager` особым образом при генерации DDL.
   ```cpp
   [[nodiscard]] const std::vector& columns() const;
   ```
   Список колонок таблицы. Не включает PK и FK — они хранятся отдельно и обрабатываются `db_manager` особым образом при генерации DDL.

- **`pk_column`** - возвращает имя PK колонки. По умолчанию `"id"`.
   ```cpp
   [[nodiscard]] const std::string& pk_column() const;
   ```

- **`has_custom_pk`** - возвращает `true`, если PK взят из данных, и `false`, если сгенерирован (`AUTOINCREMENT`).
   ```cpp
   [[nodiscard]] bool has_custom_pk() const;
   ```

   Влияет на то как `db_manager` генерирует DDL:

   ```sql
   -- has_custom_pk = false
   id INTEGER PRIMARY KEY AUTOINCREMENT

   -- has_custom_pk = true
   id INTEGER PRIMARY KEY
   ```

---

### Связи

- **`is_child`** - возвращает `true`, если таблица является дочерней — имеет родителя. Используется в `db_manager` чтобы понять нужно ли добавлять FK колонку при генерации DDL.
   ```cpp
   [[nodiscard]] bool is_child() const;
   ```
   ```cpp
   if (schema.is_child()) {
      // добавить FK колонку в CREATE TABLE
   }
   ```



- **`parent_table`** - возвращает имя родительской таблицы. Пустая строка если таблица корневая.
   ```cpp
   [[nodiscard]] const std::string& parent_table() const;
   ```


- **`foreign_key`** - возвращает имя FK колонки в этой таблице. Пустая строка если таблица корневая. Формируется как `{parent_table_name}_id`.
   ```cpp
   [[nodiscard]] const std::string& foreign_key() const;
   ```

---


### Поиск колонок

- **`find_column`** - возвращает `std::optional<Column>`. `std::nullopt` если колонка не найдена. Используется в `db_manager` при генерации DDL и при связывании данных, чтобы узнать тип колонки по имени.
   ```cpp
   [[nodiscard]] std::optional<Column> find_column(const std::string& name) const;
   ```

- **`has_column`** - возвращает `true`, если колонка с таким именем существует.
   ```cpp
   [[nodiscard]] bool has_column(const std::string& name) const;
   ```

- **`column_count`** - возвращает количество колонок. Не учитывает PK и FK.
   ```cpp
   [[nodiscard]] std::size_t column_count() const;
   ```










---










## models::data_row

### Назначение

**`data_row`** представляет одну строку данных, прочитанную из CSV или JSON, до её записи в SQLite.
Это промежуточная структура — она живёт между парсером и `db_manager`. Парсер заполняет `data_row`, `db_manager` читает из неё и вызывает `sqlite3_bind_*`.

---
   
### Расположение

```
src/
└── models/
    ├── data_row.h
    └── data_row.cpp
```

---

### Типы данных

```cpp
using Value   = std::optional<std::string>;
using Storage = std::unordered_map<std::string, Value>;
```

**`Value`** - значение одной ячейки:

   | Значение | Смысл |
   |----------|-------|
   | `std::nullopt` | NULL - отсутствие данных |
   | `std::optional{"some info"}` | обычное строковое значение |

   Решение о том является ли значение NULL принимает парсер через `string_utils::is_null_like`.  `DataRow` просто хранит то что ей передали.

**`Storage`** - публичный алиас для `unordered_map`. Используется когда парсер хочет передать все значения сразу через конструктор.

---

### Конструкторы

- **`DataRow()`** - cоздаёт пустую строку без колонок.
   ```cpp
   DataRow() = default;
   ```

- **`DataRow(Storage)`** - принимает готовую мапу. Используется когда парсер собрал все значения заранее и хочет передать их одним движением без последовательных вызовов `set`.
   ```cpp
   explicit DataRow(Storage data);
   ```

---

### Запись

- **`set`** - устанавливает значение колонки.
   ```cpp
   void set(const std::string& column, Value value);
   ```

- **`set_null`** - по сути семантический сахар для `set(column, std::nullopt)`.=
   ```cpp
   void set_null(const std::string& column);
   ```

---

### Чтение

- **`get`** - возвращает значение колонки. Если колонка отсутствует - возвращает `std::nullopt`. Не бросает исключений.
   ```cpp
   [[nodiscard]] Value get(const std::string& column) const;
   ```
   В `db_manager` мы итерируемся по колонкам из `TableSchema` и для каждой запрашиваем значение из `DataRow`. Если парсер по какой-то причине не заполнил колонку - это не фатальная ошибка, это NULL.
   Если бы `get` бросал исключение, пришлось бы оборачивать каждый вызов в try/catch.

- **`has`** - `true` если колонка присутствует в строке - даже если её значение `std::nullopt`. Позволяет различить две принципиально разные ситуации: "колонка не пришла из парсера" и "колонка пришла с NULL".
   ```cpp
   [[nodiscard]] bool has(const std::string& column) const;
   ```

- **`is_null`** - `true` если значение колонки равно NULL. Возвращает `true` и для отсутствующей колонки - отсутствие колонки трактуется как NULL. По сути синтаксический сахар над `!row.get(column).has_value()`.
   C точки зрения базы данных нет разницы между "колонка пришла с NULL" и "колонка вообще не пришла" — в обоих случаях в SQLite запишется NULL.
   ```cpp
   [[nodiscard]] bool is_null(const std::string& column) const;
   ```
   Используется в `db_manager`:
   ```cpp
   if (row.is_null(column)) {
      sqlite3_bind_null(stmt, i);
   }
   ```
- **`size`** - количество колонок в строке. Используется для валидации — убедиться что парсер заполнил столько колонок сколько есть в схеме.
   ```cpp
   [[nodiscard]] std::size_t size() const;
   ```

- **`empty`** - `true` если строка не содержит ни одной колонки.
   ```cpp
   [[nodiscard]] bool empty() const;
   ```


---

### Итерация

```cpp
[[nodiscard]] Storage::const_iterator begin() const;
[[nodiscard]] Storage::const_iterator end() const;
```
Позволяет итерироваться по всем колонкам строки

---

### Взаимодействие с другими классами

Парсер создаёт `data_row` и заполняет её через `set` / `set_null`. Имена колонок нормализует через `string_utils::to_sql_identifier` перед передачей в `set`.

`db_manager` читает из `data_row` через `get` / `is_null` при формировании `sqlite3_bind_*` вызовов. Типы колонок берёт из `table_schema` - `data_row` о типах ничего не знает.

`table_schema` и `data_row` намеренно разделены: схема описывает структуру таблицы, строка хранит конкретные данные. Это позволяет держать одну схему и много строк не дублируя информацию о типах.






---






# utils

## utils::type_converter

### Назначение

**`type_converter`** - stateless-класс (все методы статические, конструктор удалён), отвечающий за три задачи:
- **Определение типа** строковых значений из CSV/JSON файлов
- **Конвертацию** строк в типизированные значения (`sql_value`), готовые к передаче в `sqlite3_bind_*`
- **Сериализацию** значений в SQL-литералы для DDL

---
   
### Расположение

```
src/
└── utils/
    ├── type_converter.h
    └── type_converter.cpp
```

---


### Типы данных

-  **`sql_type`**

   ```cpp
   enum class sql_type : uint8_t {
      Null,
      Integer,
      Real,
      Boolean,
      Text
   };
   ```

   Перечисление отражает поддерживаемые типы колонок SQLite с точки зрения **стратегии конвертации**, а не DDL. Порядок членов соответствует приоритету автодетекции (от наиболее специфичного к наименее):

   | Значение  | DDL-тип в SQLite | Описание                                              |
   |-----------|-----------------|-------------------------------------------------------|
   | `Null`    | `TEXT`          | Все значения в колонке — null-like; дефолт — TEXT     |
   | `Integer` | `INTEGER`       | Целые числа: `"42"`, `"-7"`, `"+100"`                 |
   | `Real`    | `REAL`          | Вещественные: `"3.14"`, `"-0.5"`, `"1e10"`, `".5"`   |
   | `Boolean` | `INTEGER`       | Булевы значения: `"true/false"`, `"yes/no"`, `"1/0"`, `"on/off"` |
   | `Text`    | `TEXT`          | Всё остальное, а также финальное состояние при конфликте типов |

   `Boolean` выделен в отдельный тип, хотя SQLite хранит его как `INTEGER` поскольку `sql_type` управляет стратегией конвертации, а не только DDL. Без отдельного `Boolean` строка `"yes"` в INTEGER-колонке упала бы в fallback на `Text`, а не сконвертировалась в `int64_t(1)`.


- **`sql_value`**

   ```cpp
   using sql_value = std::variant<std::nullptr_t, int64_t, double, bool, std::string>;

   ```

   Типизированное значение, готовое к передаче в `sqlite3_bind_*`. Порядок альтернатив в `variant` намеренно совпадает с порядком членов `sql_type`:

   | Альтернатива    | Соответствует  | `sqlite3_bind_*`          |
   |-----------------|----------------|---------------------------|
   | `nullptr_t`     | `Null`         | `sqlite3_bind_null`       |
   | `int64_t`       | `Integer`      | `sqlite3_bind_int64`      |
   | `double`        | `Real`         | `sqlite3_bind_double`     |
   | `bool`          | `Boolean`      | `sqlite3_bind_int` (0/1)  |
   | `std::string`   | `Text`         | `sqlite3_bind_text`       |


---


### Интерфейс

- **`infer_type`** - определяет наиболее специфичный `sql_type` для одного строкового значения.

   ```cpp
   [[nodiscard]] static sql_type infer_type(std::string_view s);
   ```

   Порядок проверок (первое совпадение побеждает):
   ```
   1. is_null_like(s)         -->    sql_type::Null
   2. is_integer(trim(s))     -->    sql_type::Integer
   3. is_real(trim(s))        -->    sql_type::Real
   4. is_boolean(s)           -->    sql_type::Boolean
   5. иначе                   -->    sql_type::Text
   ```

- **`infer_column_type`** - выводит единый тип для **целой колонки** по всем её значениям.
   ```cpp
   [[nodiscard]] static sql_type infer_column_type(const std::vector<std::string_view>& values);
   [[nodiscard]] static sql_type infer_column_type(const std::vector<std::string>& values);
   ```

   

   Алгоритм:
      1. Начинает с `sql_type::Null`
      2. Для каждого не-null значения вызывает `infer_type` и повышает текущий тип через `promote`
      3. При достижении `Text` - прерывает обход (финальное состояние)
      4. Если все значения null-like — возвращает `Text` (безопасный дефолт для пустой колонки)

   ```cpp
   infer_column_type({"1", "2", "3"})          // -->  Integer
   infer_column_type({"1", "2", "3.5"})        // -->  Real  (Integer --> Real)
   infer_column_type({"1", "null", "3"})       // -->  Integer (null игнорируется)
   infer_column_type({"null", "null"})         // -->  Text   (все null --> дефолт)
   infer_column_type({"true", "false", "1"})   // -->  Text   (Boolean + Integer --> Text)
   infer_column_type({"hello", "world"})       // -->  Text
   ```

- **`promote`** - возвращает наименее специфичный тип, способный вместить оба. Вынесен в `public` для тестирования. Используется в `infer_column_type` для повышения типа колонки при обходе её значений.
   ```cpp
   [[nodiscard]] static sql_type promote(sql_type current, sql_type incoming);
   ```

   Таблица повышения типов:

   | Текущий \ Новый | Null | Integer | Real | Boolean | Text |
   |-----------------|------|---------|------|---------|------|
   | Null            | Null | Integer | Real | Boolean | Text |
   | Integer         | Integer | Integer | Real | Text    | Text |
   | Real            | Real  | Real    | Real  | Text    | Text |
   | Boolean         | Boolean| Text    | Text  | Boolean| Text |
   | Text            | Text  | Text    | Text  | Text    | Text |

   Ключевые правила:
   - `Null` поглощается любым конкретным типом
   - `Integer + Real -> Real` - расширение без потерь
   - `Boolean + Integer/Real -> Text` - нельзя однозначно различить `"1"` как число и `"1"` как `true`
   - `Text` - финальное состояние, из которого нет выхода


- **`convert`** (с подсказкой о типе) - конвертирует строку в `sql_value`, опираясь на заранее известный тип колонки.
   ```cpp
   [[nodiscard]] static sql_value convert(std::string_view s, sql_type col_type);
   ```
   Поведение:
   - Если `s` is_null_like - всегда возвращает `nullptr_t`, независимо от `col_type`
   - При ошибке конвертации (например, `"abc"` в `Integer`-колонке) - тихий fallback на `std::string`, исключений не бросает

   ```cpp
   convert("42",    sql_type::Integer) // --> int64_t(42)
   convert("3.14",  sql_type::Real)    // --> double(3.14)
   convert("yes",   sql_type::Boolean) // --> bool(true)
   convert("null",  sql_type::Integer) // --> nullptr  (null-like игнорирует col_type)
   convert("abc",   sql_type::Integer) // --> string("abc")  (fallback)
   convert("  42 ", sql_type::Integer) // --> int64_t(42)  (trim применяется)
   ```

- **`convert`** (автодетекция)
   ```cpp
   [[nodiscard]] static sql_value convert(std::string_view s);
   ```

   Эквивалент `convert(s, infer_type(s))`.


- **`to_sql_literal`** - возвращает SQL-литерал для генерации DDL-скриптов.

   ```cpp
   [[nodiscard]] static std::string to_sql_literal(const sql_value& value);
   ```
   > Не предназначен для подстановки в prepared statements. Для передачи параметров получаем `sql_value` через `convert()` и передаём его в `sqlite3_bind_*` в `db_manager`.


- **`sql_type_name`** - возвращает имя типа для использования в `CREATE TABLE`

   ```cpp
   [[nodiscard]] static std::string_view sql_type_name(sql_type type);
   ```

- **`type_of`** - возвращает `sql_type`, соответствующий хранимой альтернативе `variant`. Полезен при обходе результатов после конвертации.


   ```cpp
   [[nodiscard]] static sql_type type_of(const sql_value& value);
   ```


---
























## utils::file_validator

### Назначение

**`file_validator`** - утилитный класс, выполняющий предварительную проверку файла перед передачей его в парсер. Его единственная ответственность - убедиться, что файл **пригоден для чтения**: он существует, доступен, имеет ожидаемый тип и содержит текстовые данные. Класс намеренно не валидирует структуру содержимого (корректность CSV-столбцов, синтаксис JSON и т.д.) - это зона ответственности парсеров.

Цель класса - дать понятную диагностику на самом раннем этапе, до того как парсер столкнётся с неожиданным вводом.

---
   
### Расположение

```
src/
└── utils/
    ├── file_validator.h
    └── file_validator.cpp
```

---


### Структуры и типы

#### `validation_result`

Возвращается всеми методами проверки.

```cpp
struct validation_result {
    bool valid;          // true — файл прошёл проверку
    std::string error;   // описание первой обнаруженной ошибки, если valid == false

    explicit operator bool() const;               
    static validation_result ok();                 // фабрика успешного результата
    static validation_result fail(std::string);    // фабрика ошибки
};
```

Класс возвращает **первую** найденную ошибку и останавливается. Такое поведение выбрано намеренно: нет смысла проверять размер файла, которого не существует - сообщение об ошибке было бы вводящим в заблуждение.



#### `file_type`

```cpp
enum class file_type {
   CSV,
   JSON,
   UNKNOWN
};
```

Используется в `detect_type()` для определения типа файла по расширению.

---

### Интерфейс

#### Конструктор

```cpp
explicit file_validator(uintmax_t max_file_size = DEFAULT_MAX_FILE_SIZE);
```
**`max_file_size`** - единственное поле класса, задающее верхнюю границу размера обрабатываемых файлов. 

**`DEFAULT_MAX_FILE_SIZE`** - константа, задающая разумный предел для размера обрабатываемых файлов (по умолчанию 512 МБ). Этот параметр может быть переопределён при создании экземпляра `file_validator`, что позволяет адаптировать его под конкретные требования проекта.

**`BINARY_PROBE_SIZE`** - константа, определяющая количество байт, которые будут прочитаны из файла для эвристической проверки на бинарность. Обычно достаточно первых 512 байт, чтобы с высокой вероятностью определить, является ли файл текстовым или бинарным

Класс намеренно держится без состояния насколько, насколько это возможно: один экземпляр можно безопасно переиспользовать для валидации любого количества файлов, в том числе из нескольких потоков одновременно - никакого мутабельного состояния между вызовами нет.


#### Атомарные проверки и вспомогательные методы
Все проверки из цепочки доступны как отдельные публичные методы. `static` явно сигнализирует «этот метод не зависит от состояния объекта». 

```c++
[[nodiscard]] static validation_result check_path_not_empty(const std::filesystem::path& path);
[[nodiscard]] static validation_result check_exists(const std::filesystem::path& path);
[[nodiscard]] static validation_result check_is_regular_file(const std::filesystem::path& path);
[[nodiscard]] static validation_result check_readable(const std::filesystem::path& path);
[[nodiscard]] validation_result check_size(const std::filesystem::path& path) const;
[[nodiscard]] static validation_result check_extension(const std::filesystem::path& path);
[[nodiscard]] static validation_result check_not_binary(const std::filesystem::path& path);
```

**`detect_type()`** - определение типа файла по расширению. Сравнение регистронезависимо: .CSV, .Json, .csv - все вернут корректный тип.
```c++
[[nodiscard]] static file_type detect_type(const std::filesystem::path& path);
```

**`check_not_binary()`** - эвристическая проверка на бинарность. Читает первые `BINARY_PROBE_SIZE` байт файла и проверяет их на наличие нулевых байт или других не текстовых символов. 
- Нулевой байт (0x00) - немедленный вердикт - «бинарный». Он практически никогда не встречается в корректных UTF-8 текстах, но является нормой для большинства бинарных форматов.
- Управляющие символы (байты `0x01`-`0x1F`, кроме `\t`, `\n`, `\r`, `ESC`) считаются подозрительными. Если их доля превышает 30% от проверяемого фрагмента - файл считается бинарным.

Порог в 30% выбран с запасом: в реальных CSV/JSON управляющих символов практически нет, тогда как в бинарных файлах (PNG, SQLite, ZIP и т.п.) их концентрация в начале файла обычно значительно выше.

```c++
[[nodiscard]] static bool looks_binary(const char* buf, std::size_t len);
```

---

## utils::string_utils

### Назначение

`string_utils` - утилитный класс для работы со строками в контексте конвертации CSV/JSON в SQLite. Все методы статические, класс не хранит состояния.

---
   
### Расположение

```
src/
└── utils/
    ├── string_utils.h
    └── string_utils.cpp
```

---


### Интерфейс

1. **Trim / Strip** 
   - **`trim`** - обрезает пробельные символы с обеих сторон строки (' ', '\t', '\n', '\r', '\v').
      ```c++
      [[nodiscard]] static std::string trim(std::string_view s);
      ```
   
   - **`collapse_whitespace`** - cхлопывает внутренние последовательности пробельных символов в один пробел. Ведущие и хвостовые пробелы тоже убираются.
      ```c++
      [[nodiscard]] static std::string collapse_whitespace(std::string_view s);
      ```
---

2. **Split / Join**
   - **`split`(по символу)** - разбивает строку по символу-разделителю. Пустые токены сохраняются - это важно для CSV, где ,, означает пустое поле.
      ```c++
      [[nodiscard]] static std::vector<std::string> split(std::string_view s, char delimiter);
      ```

   - **`split`(по строке)** - разбивает строку по строке-разделителю. Если разделитель пустой - разбивает по символам.
      ```c++
      [[nodiscard]] static std::vector<std::string> split(std::string_view s, std::string_view delimiter);
      ```
   
   - **`join`** - соединяет вектор строк через разделитель.
      ```c++
      [[nodiscard]] static std::string join(const std::vector<std::string>& parts, std::string_view delimiter);
      ```

---

3. **Case**

   - **`to_lower` / `to_upper`** - приводит ASCII-символы к нижнему или верхнему регистру. Non-ASCII символы не затрагиваются.
      ```c++
      [[nodiscard]] static std::string to_lower(std::string_view s);
      [[nodiscard]] static std::string to_upper(std::string_view s);
      ```

---

4. **Проверки содержимого**

   - **`is_blank`** - `true` если строка пуста или состоит только из пробельных символов.

      ```c++
      [[nodiscard]] static bool is_blank(std::string_view s);
      ```

   - **`is_null_like`** - `true` если строка представляет NULL-значение. Проверка регистронезависима.
   Распознаёт: `""`, `"null"`, `"nil"`, `"none"`, `"n/a"`, `"na"`.

      ```cpp
      [[nodiscard]] static bool is_null_like(std::string_view s);
      ```

   - **`is_integer`** - `true` если строка содержит целое число. Разрешён ведущий знак `+`/`-`. Пробелы, дроби и экспоненциальная запись запрещены.

      ```cpp
      [[nodiscard]] static bool is_integer(std::string_view s);
      ```

   - **`is_boolean`** - `true` если строка представляет булево значение. Проверка регистронезависима.

      ```cpp
      [[nodiscard]] static bool is_boolean(std::string_view s);
      ```


      | true-like | false-like |
      |-----------|------------|
      | `true`    | `false`    |
      | `yes`     | `no`       |
      | `1`       | `0`        |
      | `on`      | `off`      |


   - **`is_real`** - `true` если строка содержит вещественное число. Целые числа тоже возвращают `true` (INTEGER $\in$ REAL).
      Примеры, когда будет возвращено `true`: 3.14, -0.5, +1e10, 2.5E-5, .5, 42

      ```cpp
      [[nodiscard]] static bool is_real(std::string_view s);
      ```
--- 

5. **Парсинг значений**

   - **`parse_boolean`** - парсит булево значение. Возвращает `std::nullopt` если строка не является булевой.

      ```cpp
      [[nodiscard]] static std::optional parse_boolean(std::string_view s);
      ```

   - **`unquote_json_string`** - удаляет обрамляющие кавычки и раскрывает escape-последовательности внутри JSON-строки. Если строка не обрамлена кавычками или содержит ошибку — возвращает `std::nullopt`.
   Поддерживаемые escape-последовательности: `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX` (BMP).
      ```cpp
      [[nodiscard]] static std::optional unquote_json_string(std::string_view s);
      ```

      Примеры успешного парсинга:
      ```
      Вход:  "\"hello\\nworld\""                  Выход: "hello\nworld" 
      Вход:  "hello\tworld"                       Выход: hello   world
      Вход:  "line1\nline2"                       Выход: line1
                                                         line2 
      Вход:  "caf\u00e9"                          Выход:  "café"
      Вход:  "\u0048\u0065\u006C\u006C\u006F"     Выход: Hello
      Вход:  "C:\\Users\\file"                    Выход: C:\Users\file
      ```

   - **`unquote_csv_field`** - удаляет обрамляющие кавычки CSV-поля и раскрывает удвоенные кавычки. Если поле не обрамлено кавычками — возвращает строку как есть.
      ```cpp
      [[nodiscard]] static std::string unquote_csv_field(std::string_view s);
      ```

      Примеры успешного парсинга:
      ```
      Вход:  "\"hello \"\"world\"\"\""            Выход: "hello \"world\""
      Вход:  "simple field"                       Выход: "simple field"
      ```
---

6. **Экранирование**

   - **`escape_sql_string`** - экранирует строку для безопасной вставки в SQL, удваивая одинарные кавычки.
      > **Важно:** не заменяет prepared statements. Используется только там, где параметризация невозможна — в DDL-запросах.
      ```cpp
      [[nodiscard]] static std::string escape_sql_string(std::string_view s);
      ```
      Пример:
      ```
      Вход: "O'Brien"                             Выход: "O''Brien"
      ```

   - **`quote_sql_identifier`** - оборачивает идентификатор SQLite (имя таблицы или колонки) в двойные кавычки. Внутренние двойные кавычки удваиваются.
   Используется в DDL при генерации `CREATE TABLE` из заголовков CSV, которые могут содержать пробелы, дефисы или зарезервированные слова (`order`, `group` и т.д.).

      ```cpp
      [[nodiscard]] static std::string quote_sql_identifier(std::string_view s);
      ```

      ```
      Вход:  "first name"                             Выход: "\"first name\""
      Вход:  "say \"hi\""                             Выход: "\"say \"\"hi\"\"\""
      Вход:  "order"                                  Выход: "\"order\""
      ```
---



7. **Нормализация идентификаторов**

- **`to_sql_identifier`** - превращает произвольную строку (например, заголовок CSV) в допустимое имя колонки SQLite.
   ```cpp
   [[nodiscard]] static std::string to_sql_identifier(std::string_view s);
   ```
   Алгоритм:
      1. Обрезает пробельные символы с обеих сторон (Trim)
      2. Сводит все последовательные пробельные символы к одному (collapse_whitespace)
      3. Пробелы и дефисы превращает в `'_'`
      4. Удаляет все символы кроме `[A-Za-z0-9_]`
      5. Если начинается с цифры - добавляет префикс `"col_"`
      6. Если пуста - возвращает `"col_unknown"`
      7. Преобразует в нижний регистр (to_lower)

- **`make_unique_identifier`** - генерирует уникальное имя, добавляя числовой суффикс при коллизии. Используется для сохранения уникальности заголовков в базе данных.
   ```cpp
   [[nodiscard]] static std::string make_unique_identifier(const std::string& base, const std::vector& existing);
   ```
   Пример:
   ```cpp
   std::vector used = {"name", "name_1"};

   StringUtils::make_unique_identifier("age",    used)  // --> "age"
   StringUtils::make_unique_identifier("name",   used)  // --> "name_2"
   StringUtils::make_unique_identifier("name_1", used)  // --> "name_1_1"
   ```

---



8. **Предикаты / поиск**

- **`starts_with` / `ends_with` / `contains`** - проверяют, начинается ли строка с префикса, заканчивается ли суффиксом или содержит ли подстроку. Все проверки с учётом регистра.
   ```cpp
   [[nodiscard]] static bool starts_with(std::string_view s, std::string_view prefix);
   [[nodiscard]] static bool ends_with(std::string_view s, std::string_view suffix);
   [[nodiscard]] static bool contains(std::string_view s, std::string_view needle);
   ```

- **`iequal`** - регистронезависимое сравнение двух строк.
   ```cpp
   [[nodiscard]] static bool iequal(std::string_view a, std::string_view b);
   ```

- **`replace_all`** - заменяет все вхождения подстроки `from` на `to`. Если `from` пустая — возвращает строку без изменений.
   ```cpp
   [[nodiscard]] static std::string replace_all(
      std::string_view s,
      std::string_view from,
      std::string_view to
   );
   ```

---










# Система сборки

Пусть этот раздел несколько избыточен для описания принципа работы проекта, но я решил посвятить ему достаточно много времени, так как `CMake` - это не просто способ скомпилировать код, а инструмент управления жизненным циклом проекта.

Начнём из далека, ранее я не слишком интересовался процессом сборки, и, как и многие, просто писал простенький `CmakeLists.txt` особо не задумываясь о том, что же в действительности происходит под капотом и для чего это нужно.

Прежде чем углубиться в детали реализации, стоит ответить на вопрос: а зачем вообще нужна система сборки в проекте на C++? Когда проект состоит из пары файлов, компиляцию можно выполнять и вручную. Однако по мере роста кодовой базы возникают различные проблемы: необходимость компилировать десятки файлов с правильными флагами, отслеживать зависимости между ними (чтобы пересобирать только изменившиеся части, экономя тем самым время сборки) и обеспечивать воспроизводимость сборки на разных машинах (очевидно, что принципы сборки для разных платформ будут отличаться).

Система сборки автоматизирует этот процесс. Ее ключевые функции:

1. Запуск компилятора с нужными аргументами (флагами)
   К примеру `-O2 -Wall -Iinclude -Llib -lmylib` - куча аргументов, каждый из которых что-то настраивает. Ноч то если проект одновременно использует `OpenCV`, `Boost` и `SQLite` - чтобы это скомпилировать, тебе нужно знать: где на системе лежат их заголовочные файлы, Какие именно библиотеки линковать, при этом на Windows пути будут совсем другие.
   И главное - нужно помнить это всё и вбивать руками при каждой компиляции...

2. Управление зависимостями:
   - Зависимости между файлами
     К примеру у нас есть следующие файлы:
     - `main.cpp` - главный файл программы. 
     - `math_utils.h` - заголовочный файл 
     - `math_utils.cpp` - файл с реализацией.

     Если `main.cpp` зависит от `math_utils.h` (содержит строку `#include "math_utils.h"`), тогда система сборки скомпилирует `math_utils.cpp` в `math_utils.o` и `main.cpp` в `main.o`, потом "склеит" их в программу. Тогда если в файл `math_utils.h` будут внесены изменения (добавлен комментарий), то без системы сборки пришлось бы перекомпилировать всё вручную, в то время как система сборки перекомпилирует только `main.cpp`, оставив без изменения `math_utils.cpp`
     

3. Абстрагирование от конкретной платформы. Разработчик описывает что собирать, а система сборки решает как это сделать наиболее эффективно. Он описывает проект в терминах "у меня есть: исходники, библиотеки, исполняемые файлы", а система сборки самостоятельно транслирует это описание в инструкции для конкретного компилятора и операционной системы. Например, одна и та же директива `add_library(NAME SHARED)` создаст `.dll` на Windows, `.so` на Linux и `.dylib` на macOS.


Таким образом, система сборки - это не просто инструмент для компиляции, а полноценный менеджер жизненного цикла проекта, который обеспечивает автоматизацию, оптимизацию и воспроизводимость сборки.


--- 


#### Устройство `CMakeLists.txt`
 
Файл `CMakeLists.txt` - это основной конфигурационный файл для `CMake`. В нашем проекте он выполняет следующие функции:
 
1. **Установка минимальной версии CMake и стандарта C++**:
   ```cmake
   cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
   project(DBConverter VERSION 1.0.0 LANGUAGES CXX C)
 
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   ```
   Требуется CMake 3.14+ и компилятор с поддержкой C++20.
 
2. **Директории для выходных файлов**:
   ```cmake
   set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
   set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
   set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
   ```
   Исполняемые файлы и тесты попадают в `build/bin/`, статические библиотеки в `build/lib/`.
 
3. **Подключение зависимостей**:
   ```cmake
   include(cmake/FetchSQLite.cmake)
   include(cmake/FetchJSON.cmake)
   include(cmake/FetchGTest.cmake)
   ```
   Каждая зависимость вынесена в отдельный файл в папке `cmake/`. При первой сборке CMake автоматически скачивает их из интернета.
 
4. **Список исходников**:
   ```cmake
   set(LIB_SOURCES
      ${CMAKE_SOURCE_DIR}/src/utils/file_validator.cpp
      ${CMAKE_SOURCE_DIR}/src/utils/string_utils.cpp
      ${CMAKE_SOURCE_DIR}/src/utils/type_converter.cpp
      ${CMAKE_SOURCE_DIR}/src/models/table_schema.cpp
      ${CMAKE_SOURCE_DIR}/src/models/data_row.cpp
      ${CMAKE_SOURCE_DIR}/src/database/db_manager.cpp
      ${CMAKE_SOURCE_DIR}/src/parsers/csv_parser.cpp
      ${CMAKE_SOURCE_DIR}/src/parsers/json_parser.cpp
   )
   ```
   Вынесен в отдельную переменную чтобы не дублировать между основным таргетом и тестами. Пути абсолютные через `CMAKE_SOURCE_DIR` - это важно, иначе линковщик не найдёт файлы при сборке тестов.
 
5. **Основной исполняемый файл**:
   ```cmake
   add_executable(${PROJECT_NAME} src/main.cpp ${LIB_SOURCES})
 
   target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3)
 
   target_include_directories(${PROJECT_NAME}
      PRIVATE ${SQLITE_INCLUDE_DIR}
      PRIVATE ${CMAKE_BINARY_DIR}/lib
      PRIVATE ${CMAKE_SOURCE_DIR}/src
   )
   ```
   `${CMAKE_BINARY_DIR}/lib` - путь куда скачивается `nlohmann/json.hpp`.
 
6. **Тесты**:
   ```cmake
   add_executable(DBConverter_tests
      tests/test_string_utils.cpp
      tests/test_type_converter.cpp
      tests/test_csv_parser.cpp
      tests/test_json_parser.cpp
      tests/test_db_manager.cpp
      tests/test_integration.cpp
      ${LIB_SOURCES}
   )
 
   target_link_libraries(DBConverter_tests
      PRIVATE GTest::gtest_main
      PRIVATE SQLite3
   )
 
   target_include_directories(DBConverter_tests
      PRIVATE ${SQLITE_INCLUDE_DIR}
      PRIVATE ${CMAKE_BINARY_DIR}/lib
      PRIVATE ${CMAKE_SOURCE_DIR}/src
   )
 
   target_compile_definitions(DBConverter_tests
      PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/test_data"
   )
 
   include(GoogleTest)
   gtest_discover_tests(DBConverter_tests)
   ```
   `LIB_SOURCES` компилируется вместе с тестами - тесты имеют доступ ко всем классам проекта. `TEST_DATA_DIR` - абсолютный путь к тестовым данным, передаётся как макрос препроцессора чтобы тесты находили CSV и JSON файлы независимо от рабочей директории при запуске.
 
---
 
#### Устройство `cmake/FetchSQLite.cmake`
 
Скачивает и компилирует SQLite из исходников при первой сборке. Используется SQLite amalgamation - один файл `sqlite3.c` который содержит всю библиотеку целиком.
 
```cmake
FetchContent_Declare(
   sqlite3
   URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
   DOWNLOAD_EXTRACT_TIMESTAMP TRUE
   SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/sqlite3-src
)
 
FetchContent_MakeAvailable(sqlite3)
add_library(SQLite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
```
 
Подключённые расширения SQLite:
 
| Расширение | Назначение |
|------------|------------|
| `SQLITE_ENABLE_FTS3/4/5` | Полнотекстовый поиск |
| `SQLITE_ENABLE_JSON1` | Встроенные JSON-функции |
| `SQLITE_ENABLE_RTREE` | Пространственные индексы |
| `SQLITE_ENABLE_COLUMN_METADATA` | Метаданные колонок |
| `SQLITE_ENABLE_MATH_FUNCTIONS` | Математические функции |
 
На Windows/MinGW дополнительно добавляется `-DSQLITE_OS_WIN=1` для совместимости.
 
---
 
#### Устройство `cmake/FetchJSON.cmake`
 
Скачивает единственный заголовочный файл `nlohmann/json.hpp` версии 3.11.3 и кладёт его в `build/lib/nlohmann/`:
 
```cmake
FetchContent_Declare(
   nlohmann_json
   URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
   DOWNLOAD_NO_EXTRACT TRUE
   DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/lib/nlohmann
)
 
FetchContent_MakeAvailable(nlohmann_json)
```
 
`DOWNLOAD_NO_EXTRACT TRUE` - файл не является архивом, поэтому распаковывать его не нужно. После сборки файл доступен по пути `build/lib/nlohmann/json.hpp`. Подключается в коде как `#include <nlohmann/json.hpp>`.
 
---
 
#### Устройство `cmake/FetchGTest.cmake`
 
Скачивает Google Test версии 1.14.0 и собирает его вместе с проектом:
 
```cmake
FetchContent_Declare(
   googletest
   URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
   DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
 
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
 
FetchContent_MakeAvailable(googletest)
```
 
`INSTALL_GTEST OFF` - отключает глобальную установку Google Test в систему, библиотека используется только внутри проекта. После подключения становятся доступны таргеты `GTest::gtest_main`, `GTest::gmock` и другие.
 