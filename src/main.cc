#include <iostream>
#include <string.h>
#include <vector>
#include <unordered_set>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "trie.h"
#include "thread_struct.h"

#define NUM_THREAD 40
#define my_hash(x) ((x)%NUM_THREAD)

#define FLAG_NONE 0
#define FLAG_READY 1
#define FLAG_QUERY 2
#define FLAG_END 3

TrieNode *trie;
pthread_t threads[NUM_THREAD];
ThrArg args[NUM_THREAD];

#ifdef DBG_TS
int ts = 0;
#endif

int global_flag;
int sync_val;
std::vector<std::string> res[NUM_THREAD];
const char *query_str;
int size_query;

void *thread_main(void *arg){
    ThrArg *myqueue = (ThrArg*)arg;
    int tid = myqueue->tid;
    std::vector<std::string> *myres = &res[tid];
    Operation *op;
    char first_ch;
    while(1){
        while(myqueue->head == myqueue->tail){
            if (global_flag == FLAG_NONE){
                usleep(1);
                continue;
            }
            else if (global_flag == FLAG_END){
                pthread_exit((void*)NULL);
            }
            else{
                myqueue->head = myqueue->tail = 0;
                __sync_fetch_and_add(&sync_val,1);

                std::unordered_set<std::string> exist_chk;
                std::vector<std::string> tmp;
                int size = 1 + (size_query-1) / NUM_THREAD;
                const char *start = query_str + size * tid;
                const char *end = start + size;
                if (end-query_str > size_query){
                    end = query_str + size_query;
                }
                myres->clear();
                while(global_flag == FLAG_READY) usleep(1);
                //Safe until now
                for(const char *c = start; c < end; c++){
                    if (*c == ' ')
                        c++;
                    else
                        while(c != end && *(c-1) != ' ') c++;
                    if (c == end)
                        break;
                    tmp = queryNgram(trie, c);
                    for (std::vector<std::string>::const_iterator it = tmp.begin(); it != tmp.end(); it++){
                        if (exist_chk.find(*it) == exist_chk.end()){
                            myres->emplace_back(*it);
                            exist_chk.insert(*it);
                        }
                    }
                }
                __sync_fetch_and_sub(&sync_val,1);
                while(global_flag == FLAG_QUERY) usleep(1);
            }
        }
        op = &(myqueue->operations[myqueue->head]);
        first_ch = *(op->str.begin());
        op->str.erase(op->str.begin());
        if (op->cmd == 'A'){
            addNgram(trie->next[first_ch], op->str);
        }
        else{
            delNgram(trie->next[first_ch], op->str);
        }
        myqueue->head++;
    }
    pthread_exit((void*)NULL);
}

void initThread(){
    for (int i = 0; i < NUM_THREAD; i++){
        args[i].tid = i;
        pthread_create(&threads[i], NULL, &thread_main, (void *)&args[i]);
    }
}
void destroyThread(){
    for (int i = 0; i < NUM_THREAD; i++){
        pthread_join(threads[i], NULL);
    }
}

void input(){
    std::string buf;
    while(1){
        std::getline(std::cin, buf);
        if (buf.compare("S") == 0){
            break;
        }
        else{
            addNgram(trie, buf);
        }
    }
}

void workload(){
    std::string cmd;
    std::string buf;
    printf("R\n");
    fflush(stdout);
    while(!std::cin.eof()){
        std::cin >> cmd;
        if (cmd.compare("F") == 0){
            fflush(stdout);
            std::cin >> cmd;
            if (cmd.compare("F") == 0){
                global_flag = FLAG_END;
                break;
            }
        }
        std::getline(std::cin, buf);
        if (cmd.compare("A") == 0){
            buf.erase(buf.begin());
            char x = *buf.begin();
            if (trie->next.find(x) == trie->next.end()){
                TrieNode *newNode = newTrieNode();
                newNode->next.clear();
                trie->next[x] = newNode;
            }
            ThrArg *worker = &args[my_hash(x)];
            worker->operations[worker->tail].cmd = *(cmd.begin());
            worker->operations[worker->tail].str = buf;
            __sync_synchronize();
            worker->tail++;
        }
        else if (cmd.compare("Q") == 0){
#ifdef DBG_TS
            std::cout << ++ts << " ";
#endif
            std::vector<std::string> answer;
            std::unordered_set<std::string> exist_chk;

            query_str = buf.c_str();
            size_query = buf.size();
            __sync_synchronize();
            global_flag = FLAG_READY;
            while(sync_val != NUM_THREAD) usleep(1);
            global_flag = FLAG_QUERY;
            while(sync_val != 0) usleep(1);

            for (int i = 0; i < NUM_THREAD; i++){
                for (std::vector<std::string>::const_iterator it = res[i].begin(); it != res[i].end(); it++){
                    if (exist_chk.find(*it) == exist_chk.end()){
                        answer.emplace_back(*it);
                        exist_chk.insert(*it);
                    }
                }
            }

            if (answer.size()){
                std::cout << answer[0];
                for (std::vector<std::string>::const_iterator it = ++answer.begin(); it != answer.end(); it++){
                    std::cout << "|" << *it;
                }
            }
            else{
                std::cout << -1;
            }
            std::cout << std::endl;
            global_flag = FLAG_NONE;
        }
        else{
            buf.erase(buf.begin());
            ThrArg *worker = &args[my_hash(*buf.begin())];
            worker->operations[worker->tail].cmd = *(cmd.begin());
            worker->operations[worker->tail].str = buf;
            __sync_synchronize();
            worker->tail++;
        }
    }
}

int main(int argc, char *argv[]){
    std::ios::sync_with_stdio(false);
    initTrie(&trie);
#ifdef DBG_LOG
    std::cerr<<"initTree done" << std::endl;
#endif
    initThread();
#ifdef DBG_LOG
    std::cerr<<"initThread done" << std::endl;
#endif
    input();
#ifdef DBG_LOG
    std::cerr<<"input done" << std::endl;
#endif
    workload();
#ifdef DBG_LOG
    std::cerr<<"workload done" << std::endl;
#endif
    destroyThread();
#ifdef DBG_LOG
    std::cerr<<"destroyThread done" << std::endl;
#endif
    destroyTrie(trie);
#ifdef DBG_LOG
    std::cerr<<"destroyTrie done" << std::endl;
#endif
    return 0;
}
