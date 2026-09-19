"""Mirror of the C++ CLDR plural rules to sanity-check the expected categories."""


def category(lang, count):
    n = abs(count)
    i = int(n)
    is_int = n == float(i)
    mod10 = i % 10
    mod100 = i % 100

    if lang == "en":
        return "one" if (is_int and i == 1) else "other"
    if lang == "ru":
        if is_int and mod10 == 1 and mod100 != 11:
            return "one"
        if is_int and 2 <= mod10 <= 4 and not (12 <= mod100 <= 14):
            return "few"
        if is_int and (mod10 == 0 or 5 <= mod10 <= 9 or 11 <= mod100 <= 14):
            return "many"
        return "other"
    if lang == "ar":
        if is_int and i == 0:
            return "zero"
        if is_int and i == 1:
            return "one"
        if is_int and i == 2:
            return "two"
        if is_int and 3 <= mod100 <= 10:
            return "few"
        if is_int and 11 <= mod100 <= 99:
            return "many"
        return "other"
    return "one" if (is_int and i == 1) else "other"


cases = {
    "en": [0, 1, 2, 5, 1.5],
    "ru": [1, 2, 3, 4, 5, 11, 12, 14, 21, 22, 25, 100, 101, 102, 111],
    "ar": [0, 1, 2, 3, 10, 11, 99, 100, 101, 102, 103, 200],
}
for lang, counts in cases.items():
    print(lang, [(c, category(lang, c)) for c in counts])
