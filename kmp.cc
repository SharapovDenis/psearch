#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


using namespace std;

struct sfi {
    string file_name;
    off_t file_size;
    unsigned char file_type;
};

struct kmp {
    vector<vector<int>> table;
    size_t lenght;
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

    const int alphabet = 127; // мощность алфавита символов, обычно меньше
 
    s += '#';
    int n = (int) s.length();

    struct kmp ret_aut;
    vector<int> pi = prefix_function (s);
    vector < vector<int> > aut (n, vector<int> (alphabet));
    for (int i=0; i<n; ++i) {
        for (char c = 65; c<alphabet; ++c) {
            if (i > 0 && c != s[i])
                aut[i][c] = aut[pi[i-1]][c];
            else
                aut[i][c] = i + (c == s[i]);

            printf("%d ", aut[i][c]);
        }
        printf("\n");
    }

    ret_aut.table = aut;
    ret_aut.lenght = s.size() - 1;

    return ret_aut;

}

// внимание на оператор if!!!

int check_text(struct kmp aut, const char *text) {

    int j = 0, i = 0;

    for(i = 0; i < strlen(text); ++i) {

        if(text[i] < 65 && text[i] > 127) {
            printf("text[%d] symbol not found!\n", i);
            j = 0;
            continue;
        }

        if(j > aut.lenght) {
            printf("j problem!\n");
            return 0;
        }

        j = aut.table[j][text[i]];
        if(j == aut.lenght) {
            return j;
        }
    }
    return 0;
}

void file_reading(string file_path, struct kmp aut) {

    char *line_buf = NULL;
    size_t line_buf_size = 0;
    int line_count = 1;
    ssize_t line_size;

    FILE *fp = fopen(file_path.c_str(), "r");

    if (!fp) {
    fprintf(stderr, "Error opening file '%s'\n", file_path.c_str());
    }

    line_size = getline(&line_buf, &line_buf_size, fp);

    int flag = 0;

    while (line_size >= 0){

        flag = check_text(aut, line_buf);

        if(flag) {
            printf("file: %s line: %d text: %s", file_path.c_str(), line_count, line_buf);
        }

        line_count++;
        line_size = getline(&line_buf, &line_buf_size, fp);

    }

    free(line_buf);
    line_buf = NULL;
    fclose(fp); 

}

bool comp_descending(const struct sfi &arg1, const struct sfi &arg2) {
    return arg1.file_size > arg2.file_size;
}

void distribute(vector<struct sfi> dirs, int n) {

    int distr[n] = {0};

    int *min_pos, i;

    for(i = 0; i < dirs.size(); ++i) {
        min_pos = min_element(distr, distr + n);
        *min_pos += dirs[i].file_size;
    }

    for(i = 0; i < n; ++i) {
        printf("distr[%d]=%d\n", i, distr[i]);
    }

}

int main(int argc, char** argv) {

    string s = argc > 1 ? string(argv[1]) : "abbab";

    vector<int> pi = prefix_function(s);
    vector<struct sfi> dirs;

    dirs = walk("/home/shdenis/Desktop/prog/C/psearch");

    if(dirs.size() == 0) {
        printf("can't find directory!\n");
        return 0;
    }

    sort(dirs.begin(), dirs.end(), comp_descending);

    // в этой функции запихивать имена файлов вместе с суммарным размером
    distribute(dirs, 4);


    for(int i = 0; i < pi.size(); ++i) {
        printf("p[%d]=%d\n", i, pi[i]);
    }

    for(int i = 0; i < dirs.size(); ++i) {
        printf("dir[%d]='%s'\n", i, dirs[i].file_name.c_str());
        printf("dir[%d]='%ld'\n", i, dirs[i].file_size);
        printf("dir[%d]='%d'\n", i, dirs[i].file_type);
    }

    struct kmp aut = create_kmp(s);

    for(int i = 0; i < dirs.size(); ++i) {
        file_reading(dirs[i].file_name, aut);
    }

    //printf("flag=%d\n", flag);


    return 0;
}

// переписать бяку с ret_aut
// придумать новый символ вместо #
// внимание на оператор if в функции check_text!!!
// distribute: смотри main, там коммент
// check_text: какая-то проблема с j
