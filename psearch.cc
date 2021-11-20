#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

#define ALPHABET_START  32      /* space */
#define ALPHABET_END    125     /* '}' */    
#define END_MARKER      '~'     /* (char) 126 */

struct sfs {
    /* 
        searching files system
    */
    string file_name;
    off_t file_size;
    unsigned char file_type;
};

struct kmp {
    vector<vector<int>> table;
    size_t lenght;
};

struct argums {
    vector<string> files_path;  /* Paths to each file */
    struct kmp aut;             /* KMP */
    off_t files_size;           /* Summary size of files */
    pthread_mutex_t mutex;      /* Mutex */
};

void walk_recursive(string const &dirname, vector<struct sfs> &ret) {

    /*

        Рекурсивный поиск файлов в директории.

    */

    DIR *dir = opendir(dirname.c_str());
    struct sfs local;
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

vector<struct sfs> walk(string const &dirname) {

    /*

        Вывод в вектор файлов директории.

    */

    vector<struct sfs> ret;
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

int check_text(struct kmp aut, const char *text) {

    /*

        Launches the KMP-machine that calculates transitions (delta-function)
        and returns value > 0 when the first entry of the pattern has been reached.
        If it is not, returns 0. 

    */

    int i, j = 0;

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

    /*

        Reads files from arg and then launches KMP-machine 
        with the check_text() function.

    */

    struct argums *args = (struct argums *) arg; 
    int i;

    for(i = 0; i < args->files_path.size(); ++i) {

        char *line_buf = NULL;
        size_t line_buf_size = 0;
        int line_count = 1;
        ssize_t line_size;

        FILE *fp = fopen(args->files_path[i].c_str(), "r");

        if (!fp) {
            pthread_mutex_lock(&(args->mutex));
            fprintf(stderr, "Error opening file '%s'\n", args->files_path[i].c_str());
            pthread_mutex_unlock(&(args->mutex));
            continue;
        }

        line_size = getline(&line_buf, &line_buf_size, fp);

        int flag = 0;

        while(line_size >= 0) {

            flag = check_text(args->aut, line_buf);

            if(flag) {
                pthread_mutex_lock(&(args->mutex));
                printf("file: %s line: %d text: %s", args->files_path[i].c_str(), line_count, line_buf);
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

bool comp_descending(const struct sfs &arg1, const struct sfs &arg2) {
    return arg1.file_size > arg2.file_size;
}

vector<struct argums> distribute(vector<struct sfs> dirs, int n) {

    /*

        Distributes files by the files size into n groups.

    */

    vector<struct argums> args (n);
    vector<int> f_sizes (n, 0);
    int pos, i;

    for(i = 0; i < dirs.size(); ++i) {
        pos = min_element(f_sizes.begin(), f_sizes.end()) - f_sizes.begin();
        f_sizes[pos] += dirs[i].file_size;
        args[pos].files_path.push_back(dirs[i].file_name);
        args[pos].files_size += dirs[i].file_size;
    }

    return args;
}

void find_keys(int argc, char **argv, vector<int> *positions, string *drct) {

    /*

        Finds entries of keys in argv.    

    */

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

    /*

        Process of parsing argv line.

    */

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

    vector<struct sfs> dirs;
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
    vector<struct argums> args;

    sort(dirs.begin(), dirs.end(), comp_descending);

    args = distribute(dirs, THREADS);

    pthread_t *threads = (pthread_t *) malloc(THREADS * sizeof(pthread_t));
    pthread_mutex_t mutex_main;

    pthread_mutex_init(&mutex_main, NULL);

    for(i = 0; i < THREADS; ++i) {
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
