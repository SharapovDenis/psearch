#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

#define ALPHABET_START  65
#define ALPHABET_END    127

struct sfi {
    string file_name;
    off_t file_size;
    unsigned char file_type;
};

struct kmp {
    vector<vector<int>> table;
    size_t lenght;
};

struct distr {
    vector<string> s_files;
    off_t files_size;
};

struct argums {
    vector<string> file_path;
    struct kmp aut;
    pthread_mutex_t mutex;
};

void walk_recursive(string const &dirname, vector<struct sfi> &ret) {

    /*

        Рекурсивный поиск файлов в директории.

    */

    DIR *dir = opendir(dirname.c_str());
    struct sfi local;
    struct stat statbuf;


    if (dir == nullptr) {
        perror(dirname.c_str());
        return;
    }
    for (dirent *de = readdir(dir); de != nullptr; de = readdir(dir)) {
        if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0) continue; // не берём . и ..
        local.file_name = dirname + "/" + de->d_name;
        stat(local.file_name.c_str(), &statbuf);
        local.file_size = statbuf.st_size;
        local.file_type = de->d_type;
        ret.push_back(local); // добавление в вектор
        if (de->d_type == DT_DIR) {
            walk_recursive(dirname + "/" + de->d_name, ret);
        }
    }
    closedir(dir);
}

vector<struct sfi> walk(string const &dirname) {

    /*

        Вывод в вектор файлов директории.

    */

    vector<struct sfi> ret;
    walk_recursive(dirname, ret);

    // удаляем директории из вектора
    for(int i = 0; i < ret.size(); ++i) {
        if(ret[i].file_type == DT_DIR) {
            ret.erase(ret.begin() + i);
            --i;
        }
    }

    return ret;
}

vector<int> prefix_function (string s) {
	int n = (int) s.length();
	vector<int> pi (n);

	for (int i = 1; i < n; ++i) {
		int j = pi[i-1];
		while (j > 0 && s[i] != s[j])
			j = pi[j - 1];
		if (s[i] == s[j])  ++j;
		pi[i] = j;
	}
	return pi;
}

// переписать бяку с ret_aut
// придумать новый символ вместо #

struct kmp create_kmp(string s) {

    const int alphabet = ALPHABET_END; // мощность алфавита символов, обычно меньше
 
    s += '#';
    int n = (int) s.length();

    struct kmp ret_aut;
    vector<int> pi = prefix_function (s);
    vector < vector<int> > aut (n, vector<int> (alphabet+1));
    for (int i=0; i < n; ++i) {
        for (char c = ALPHABET_START; c < alphabet; ++c) {
            if (i > 0 && c != s[i])
                aut[i][c] = aut[pi[i-1]][c];
            else
                aut[i][c] = i + (c == s[i]);
        }
    }

    ret_aut.table = aut;
    ret_aut.lenght = s.size() - 1;

    return ret_aut;

}

int check_text(struct kmp aut, const char *text) {

    int j = 0, i = 0;

    for(i = 0; i < strlen(text); ++i) {

        if(text[i] < ALPHABET_START || text[i] > ALPHABET_END) {
            j = 0;
            continue;
        }

        if(j > aut.lenght) {
            j = 0;
            continue;
        }

        j = aut.table[j][text[i]];

        if(j == aut.lenght) {
            return j;
        }
        
    }
    return 0;
}

void *file_reading(void *arg) {

    struct argums *args = (struct argums *) arg; 

    int i;

    for(i = 0; i < args->file_path.size(); ++i) {

        char *line_buf = NULL;
        size_t line_buf_size = 0;
        int line_count = 1;
        ssize_t line_size;

        FILE *fp = fopen(args->file_path[i].c_str(), "r");

        if (!fp) {
            pthread_mutex_lock(&(args->mutex));
            fprintf(stderr, "Error opening file '%s'\n", args->file_path[i].c_str());
            pthread_mutex_unlock(&(args->mutex));
            continue;
        }

        line_size = getline(&line_buf, &line_buf_size, fp);

        int flag = 0;

        while(line_size >= 0) {

            flag = check_text(args->aut, line_buf);

            if(flag) {
                pthread_mutex_lock(&(args->mutex));
                printf("file: %s line: %d text: %s", args->file_path[i].c_str(), line_count, line_buf);
                pthread_mutex_unlock(&(args->mutex));
            }

            line_count++;
            line_size = getline(&line_buf, &line_buf_size, fp);

        }

        free(line_buf);
        line_buf = NULL;
        fclose(fp); 

    }

    return NULL;
}

bool comp_descending(const struct sfi &arg1, const struct sfi &arg2) {
    return arg1.file_size > arg2.file_size;
}

vector<struct distr> distribute(vector<struct sfi> dirs, int n) {

    vector<struct distr> thrds (n);
    vector<int> f_sizes (n, 0);
    int pos, i;

    for(i = 0; i < dirs.size(); ++i) {
        pos = min_element(f_sizes.begin(), f_sizes.end()) - f_sizes.begin();
        f_sizes[pos] += dirs[i].file_size;
        thrds[pos].s_files.push_back(dirs[i].file_name);
        thrds[pos].files_size += dirs[i].file_size;
    }

    return thrds;
}

void find_keys(int argc, char **argv, vector<int> *positions, string *drct) {

    int i;

    for(i = 0; i < argc; ++i) {
        if(argv[i][0] == '-') {
            positions->push_back(i);
        }
        if(i > 1 && argv[i][0] != '-') {
            *drct = string(argv[i]);
        }
    }   
}

int parsing(int argc, char **argv, int *thr_amnt, int *dir_flag, string *drct) {

    int i, pos_size;
    char* ptr;
    vector<int> pos;

    find_keys(argc, argv, &pos, drct);
    pos_size = pos.size();

    if(pos_size > 2) {
        fprintf(stderr, "too many keys!\n");
        return -1;
    }

    for(i = 0; i < pos.size(); ++i) {

        if(strlen(argv[pos[i]]) < 2) {
            fprintf(stderr, "syntax error!\n");
            return -1;
        }

        if(argv[pos[i]][1] == 't' && strlen(argv[pos[i]]) < 3) {
            fprintf(stderr, "syntax error!\n");
            return -1;
        }

        if(argv[pos[i]][1] == 't') {
            ptr = &argv[pos[i]][1];
            *thr_amnt = atoi(ptr + 1);
        }

        if(argv[pos[i]][1] == 'n') {
            *dir_flag = 1;
        }
    }

    if(*dir_flag == 1) {
        *drct = "./";
    } 

    return 0;
}

int main(int argc, char** argv) {

    if(argc < 2) {
        fprintf(stderr, "too few arguments!\n");
        return 0;
    }

    if(argc > 4) {
        fprintf(stderr, "too many arguments!\n");
        return 0;
    }

    vector<struct sfi> dirs;
    vector<struct distr> to_thrds;
    string pattern;
    string searching_drct = "";
    int THREADS = 1, dir_flag = 0, flag = -1;
    int i;

    pattern = string(argv[1]);

    flag = parsing(argc, argv, &THREADS, &dir_flag, &searching_drct);

    if(flag == -1) {
        return 0;
    }

    dirs = walk(searching_drct);

    if(dirs.size() == 0) {
        fprintf(stderr, "can't find directory!\n");
        return 0;
    }

    struct kmp aut = create_kmp(pattern);

    sort(dirs.begin(), dirs.end(), comp_descending);

    to_thrds = distribute(dirs, THREADS);

    pthread_t *threads = (pthread_t *) malloc(THREADS * sizeof(pthread_t));
    pthread_mutex_t mutex_main;

    vector<struct argums> args (THREADS);

    pthread_mutex_init(&mutex_main, NULL);

    for(i = 0; i < THREADS; ++i) {
        args[i].file_path = to_thrds[i].s_files;
        args[i].aut = aut;
        args[i].mutex = mutex_main;
        pthread_create(threads+i, NULL, file_reading, &args[i]);
    }

    for(i = 0; i < THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex_main);
    free(threads);

    return 0;
}

// переписать бяку с ret_aut
// придумать новый символ вместо #
// внимание на оператор if в функции check_text!!!
// file_reading --- непонятно что с переносом строки (делать или нет);
// исправить argc < 2 в main (должен искать в текущей директории).
