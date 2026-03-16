import json


# Помогает конвертировать JSON с объектом объектов в массив объектов
# Вместо 'name_of_file.json' укажите имя вашего файла, который нужно конвертировать
# при необходимости отредактируйте путь к файлу (../examples/name_of_file.json) в зависимости от расположения вашего файла относительно этого скрипта
with open("../examples/name_of_file.json", "r", encoding="utf-8") as f:
    data = json.load(f)

result = []
for name, stats in data.items():
    entry = {"name": name}
    entry.update(stats)
    result.append(entry)

with open("../examples/name_of_file.json", "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"Done. {len(result)} entries.")