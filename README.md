# psearch

Функция `psearch [pattern] [-keys] [directory]` ищет вхождения шаблона `pattern` в `directory` и глубже.

Ключи keys:

<pre>

-t#     запустить поиск в # потоков

-n      поиск только в текущей директории
</pre>

## Теория:

Функция `psearch` основана на работе алгоритма Кнута-Морриса-Пратта (КПМ-алгоритм). Основная идея
алгоритма заключается в вычислении префикс-функции:

```
vector<int> prefix_function (string s) {

    /*

        Calculates the prefixes of a string s.

    */

	int i, j, n = (int) s.length();
	vector<int> pi (n);

    for (i = 1; i < n; ++i) {
        j = pi[i - 1];
        while (j > 0 && s[i] != s[j]) {
            j = pi[j - 1];
        }
        if (s[i] == s[j]){
            ++j;
        }
        pi[i] = j;
    }
    return pi;
}
```

С помощью префикс-функции строится КМП-автомат (таблица переходов) за `O(nk)`, где `n` --- длина входной строки `pattern`, `k` --- мощность алфавита:

```
#define ALPHABET_START  32      /* space */
#define ALPHABET_END    125     /* '}' */    
#define END_MARKER      '~'     /* (char) 126 */

struct kmp {
    vector<vector<int>> table;
    size_t lenght;
};

struct kmp create_kmp(string s) {
 
    /*

        Creates a KMP-machine for string s using prefix_function().

    */
    
    s += END_MARKER;
    int i, n = (int) s.length();
    char c;

    struct kmp aut;
    vector<int> pi = prefix_function(s);
    vector<vector<int>> aut_init (n, vector<int> (ALPHABET_END + 1));

    aut.table = aut_init;

    for(i = 0; i < n; ++i) {
        for(c = ALPHABET_START; c < ALPHABET_END; ++c) {
            if(i > 0 && c != s[i])
                aut.table[i][c] = aut.table[pi[i-1]][c];
            else
                aut.table[i][c] = i + (c == s[i]);
        }
    }

    aut.lenght = s.size() - 1;

    return aut;
}
```

## Компиляция:

с помощью команды make:

    make compiling

или вручную:

    c++ psearch.cc -lpthread -o psearch

## Примеры выполнения:

<pre>
Input:  time ./psearch pop -t1 /usr/include
Output: real 0m0.858s user 0m0.834s sys 0m0.024s

Input:  time ./psearch pop -t2 /usr/include
Output: real 0m0.482s user 0m0.913s sys 0m0.024s

Input:  time ./psearch pop -t3 /usr/include
Output: real 0m0.392s user 0m1.066s sys	0m0.020s

Input:  time ./psearch pop -t4 /usr/include
Output: real 0m0.322s user 0m1.194s sys 0m0.016s

Input:  time ./psearch pop -t8 /usr/include
Output: real 0m0.294s user 0m1.928s sys 0m0.037s
</pre>

## Литература:

* https://e-maxx.ru/algo/prefix_function

## Версии:

==1.0.0== 

* маркер конца строки '#' и уменьшенный алфавит.

==1.0.1== 

* маркер конца строки '~' и расширенный алфавит.

==1.0.2== 

* code styling.
