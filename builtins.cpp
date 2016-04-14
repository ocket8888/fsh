#include "builtins.h"
#include <unistd.h>
#include <map>
#include <readline/history.h>
using namespace std;


extern map<string,string> aliases;

int com_clear(vector<string>& tokens){
  printf("\033[2J\033[1;1H");
  return 0;
}

int com_ls(vector<string>& tokens) {
  // if no directory is given, use the local directory
  if (tokens.size() < 2) {
    tokens.push_back(".");
  }

  // open the directory
  DIR* dir = opendir(tokens[1].c_str());

  // catch an errors opening the directory
  if (!dir) {
    // print the error from the last system call with the given prefix
    perror("ls error: ");

    // return error
    return 1;
  }// output each entry in the directory
  for (dirent* current = readdir(dir); current; current = readdir(dir)) {
    printf("%s\n",current->d_name);
  }
  return 0;
}


int com_cd(vector<string>& tokens) {
  if(chdir(tokens[1].c_str())!=0){
    perror(tokens[1].c_str());
    return -1;
  }
  return 0;
}


int com_pwd(vector<string>& tokens) {
  printf("%s\n",pwd().c_str());
  return 0;
}


int com_alias(vector<string>& tokens) {
  if(tokens.size()==1){
    for(map<string,string>::iterator it = aliases.begin(); it != aliases.end(); ++it) {
      printf("%s = %s\n",it->first.c_str(),it->second.c_str());
    }
    return 0;
  }
  string firstbit="";
  unsigned int i=0;
  for(;i<tokens[1].length();i++){
    if(tokens[1][i]!='='){
      firstbit+=tokens[1][i];
    }
    else break;
  }
  if(i==tokens[1].length()){
    printf("alias: error, expected assignment\n");
    return 1;
  }
  i++;
  string secondbit="";
  for(;i<tokens[1].length();i++){
    secondbit+=tokens[1][i];
  }
  aliases[firstbit]=secondbit;
  return 0;
}


int com_unalias(vector<string>& tokens) {
  if(tokens[1]!="-a"){
    for(unsigned int i=1;i<tokens.size();i++){
      if(aliases.count(tokens[i])==0){
        printf("unalias: Error, alias %s not found\n",tokens[i].c_str());
        return 1;
      }
      else if(tokens[i]=="-a"){
        printf("unalias: Error, -a option should not be used with alias names.\n");
        return 2;
      }
      aliases.erase(tokens[i]);
    }
    return 0;
  }
  else if(tokens.size()!=2){
    printf("unalias: Error, -a option should not be used with alias names.\n");
    return 2;
  }
  aliases.clear();
  return 0;
}


int com_echo(vector<string>& tokens) {
if(tokens.size()>1){
  printf("%s\n",tokens[1].c_str());
return 0;
}
return 0;
}


int com_exit(vector<string>& tokens) {
  exit(0);
  return 0;
}


int com_history(vector<string>& tokens) {
  printf("History size (bytes) %i\n",history_total_bytes());
  HIST_ENTRY** histlist=history_list();
  for(int i=0;i<history_length;i++){
    printf("%s\n",histlist[i]->line);
  }
  return 0;
}

string pwd() {
  char cwd[1024];
  getcwd(cwd,sizeof(cwd));
  return cwd;
}
