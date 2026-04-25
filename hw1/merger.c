#include "common.h"
#include "merger_parser.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, 0, fd)

char **get_lines(FILE *f, int *number_of_lines)
{
  char **lines = NULL;
  int count = 0;
  int capacity = 0;
  char buf[MAX_LINE_SIZE];
  while(fgets(buf, MAX_LINE_SIZE, f)) {
    if(count == capacity) {
      capacity = capacity ? capacity * 2 : 64;
      char **tmp = realloc(lines, capacity * sizeof(char *));
      if(!tmp){ perror("realloc"); exit(1); }
      lines = tmp;
    }
    lines[count] = strdup(buf);
    if(!lines[count]){ perror("strdup"); exit(1); }
    count++;
  }
  *number_of_lines = count;
  return lines;
}

// sub-spec'i recursive olarak fd'ye yazar
void write_spec(int fd, merger_node_t *node)
{
  dprintf(fd, "%d\n", node->num_chains);
  for(int j=0; j<node->num_chains; j++){
    operator_chain_t *sc = &node->chains[j];
    dprintf(fd, "%d %d", sc->start_line, sc->end_line);

    if(sc->merger_child != NULL){
      // bu chain de bir merger
      dprintf(fd, " merger\n");
      write_spec(fd, sc->merger_child); // recursive
    } else {
      for(int k=0; k<sc->num_ops; k++){
        if(k==0) dprintf(fd, " ");
        else     dprintf(fd, " | ");
        if(sc->ops[k].type == OP_SORT)        dprintf(fd, "sort");
        else if(sc->ops[k].type == OP_FILTER) dprintf(fd, "filter");
        else if(sc->ops[k].type == OP_UNIQUE) dprintf(fd, "unique");
        dprintf(fd, " -c %d", sc->ops[k].column);
        if(sc->ops[k].col_type == TYPE_TEXT)       dprintf(fd, " -t text");
        else if(sc->ops[k].col_type == TYPE_NUM)   dprintf(fd, " -t num");
        else if(sc->ops[k].col_type == TYPE_DATE)  dprintf(fd, " -t date");
        if(sc->ops[k].type == OP_SORT && sc->ops[k].reverse)
          dprintf(fd, " -r");
        if(sc->ops[k].type == OP_FILTER){
          if(sc->ops[k].cmp == CMP_GT)       dprintf(fd, " -g");
          else if(sc->ops[k].cmp == CMP_LT)  dprintf(fd, " -l");
          else if(sc->ops[k].cmp == CMP_EQ)  dprintf(fd, " -e");
          else if(sc->ops[k].cmp == CMP_GE)  dprintf(fd, " -ge");
          else if(sc->ops[k].cmp == CMP_LE)  dprintf(fd, " -le");
          else if(sc->ops[k].cmp == CMP_NE)  dprintf(fd, " -ne");
          dprintf(fd, " %s", sc->ops[k].cmp_value);
        }
      }
      dprintf(fd, "\n");
    }
  }
}

void run_merger(merger_node_t *node, char **lines, int num_lines)
{
  int op_counts[MAX_CHAINS];
  pid_t op_pids[MAX_CHAINS][MAX_OPERATORS];
  int exit_statuses[MAX_CHAINS][MAX_OPERATORS];
  memset(exit_statuses, 0, sizeof(exit_statuses));
  memset(op_counts, 0, sizeof(op_counts));
  memset(op_pids, 0, sizeof(op_pids));

  int fd_feed[MAX_CHAINS][2];
  int fd_output[MAX_CHAINS][2];

  // başta hepsini -1 ile işaretle
  for(int i=0; i<MAX_CHAINS; i++){
    fd_feed[i][0] = fd_feed[i][1] = -1;
    fd_output[i][0] = fd_output[i][1] = -1;
  }

  for(int i=0; i<node->num_chains; i++){
    if(PIPE(fd_feed[i]) == -1){
      perror("PIPE fd_feed");
      fprintf(stderr, "fd_feed[%d] creation failed\n", i);
      exit(1);
    }
    if(PIPE(fd_output[i]) == -1){
      perror("PIPE fd_output");
      fprintf(stderr, "fd_output[%d] creation failed\n", i);
      exit(1);
    }
  }

  for(int i=0; i<node->num_chains; i++){
    operator_chain_t *chain = &node->chains[i];

    if(chain->merger_child != NULL){
      int p_from_sub[2];
      if(PIPE(p_from_sub) == -1){
        perror("sub-merger pipe failed");
        exit(1);
      }

      pid_t pid = fork();
      if(pid == -1){ perror("fork failed"); exit(1); }

      if(pid == 0){ // child — sub-merger
        dup2(p_from_sub[1], STDOUT_FILENO);
        close(p_from_sub[0]); close(p_from_sub[1]);
        for(int k=0; k<node->num_chains; k++){
          if(fd_feed[k][0] != -1) close(fd_feed[k][0]);
          if(fd_feed[k][1] != -1) close(fd_feed[k][1]);
          if(fd_output[k][0] != -1) close(fd_output[k][0]);
          if(fd_output[k][1] != -1) close(fd_output[k][1]);
        }
        int sub_start = chain->start_line - 1;
        if(sub_start < 0) sub_start = 0;
        int sub_end = chain->end_line;
        if(sub_end > num_lines) sub_end = num_lines;
        int sub_count = sub_end - sub_start;
        if(sub_count < 0) sub_count = 0;
        run_merger(chain->merger_child, &lines[sub_start], sub_count);
        exit(0);
      }

      // parent
      close(p_from_sub[1]);

      // fd_output[i] slotunu p_from_sub[0] ile değiştir
      if(fd_output[i][0] != -1) close(fd_output[i][0]);
      if(fd_output[i][1] != -1) close(fd_output[i][1]);
      fd_output[i][0] = p_from_sub[0];
      fd_output[i][1] = -1; // stale işareti

      // feed pipe'ı kapat, sub-merger kullanmıyor
      if(fd_feed[i][0] != -1){ close(fd_feed[i][0]); fd_feed[i][0] = -1; }
      if(fd_feed[i][1] != -1){ close(fd_feed[i][1]); fd_feed[i][1] = -1; }

      op_counts[i] = 0; // sub-merger EXIT-STATUS yazdırılmaz
      op_pids[i][0] = pid;

    } else {
      int num_ops = chain->num_ops;
      op_counts[i] = num_ops;
      int mid_pipes[MAX_OPERATORS][2];

      for(int j=0; j<num_ops-1; j++){
        if(PIPE(mid_pipes[j]) == -1){
          fprintf(stderr, "mid_pipes[%d] creation failed\n", j);
          exit(1);
        }
      }

      for(int j=0; j<num_ops; j++){
        pid_t pid = fork();
        if(pid == -1){ fprintf(stderr, "fork failed\n"); exit(1); }

        if(pid == 0){ // child — operatör
          if(j==0)
            dup2(fd_feed[i][0], STDIN_FILENO);
          else
            dup2(mid_pipes[j-1][0], STDIN_FILENO);

          if(j == num_ops-1)
            dup2(fd_output[i][1], STDOUT_FILENO);
          else
            dup2(mid_pipes[j][1], STDOUT_FILENO);

          for(int k=0; k<num_ops-1; k++){
            close(mid_pipes[k][0]);
            close(mid_pipes[k][1]);
          }
          for(int k=0; k<node->num_chains; k++){
            if(fd_feed[k][0] != -1) close(fd_feed[k][0]);
            if(fd_feed[k][1] != -1) close(fd_feed[k][1]);
            if(fd_output[k][0] != -1) close(fd_output[k][0]);
            if(fd_output[k][1] != -1) close(fd_output[k][1]);
          }

          char *exec_argv[32];
          int idx = 0;
          char col_buf[16];

          if(chain->ops[j].type == OP_SORT)        exec_argv[idx++] = "./sort";
          else if(chain->ops[j].type == OP_FILTER) exec_argv[idx++] = "./filter";
          else if(chain->ops[j].type == OP_UNIQUE) exec_argv[idx++] = "./unique";

          snprintf(col_buf, sizeof(col_buf), "%d", chain->ops[j].column);
          exec_argv[idx++] = "-c";
          exec_argv[idx++] = col_buf;

          exec_argv[idx++] = "-t";
          if(chain->ops[j].col_type == TYPE_TEXT)       exec_argv[idx++] = "text";
          else if(chain->ops[j].col_type == TYPE_NUM)   exec_argv[idx++] = "num";
          else if(chain->ops[j].col_type == TYPE_DATE)  exec_argv[idx++] = "date";

          if(chain->ops[j].type == OP_SORT && chain->ops[j].reverse)
            exec_argv[idx++] = "-r";

          if(chain->ops[j].type == OP_FILTER){
            if(chain->ops[j].cmp == CMP_GT)       exec_argv[idx++] = "-g";
            else if(chain->ops[j].cmp == CMP_LT)  exec_argv[idx++] = "-l";
            else if(chain->ops[j].cmp == CMP_EQ)  exec_argv[idx++] = "-e";
            else if(chain->ops[j].cmp == CMP_GE)  exec_argv[idx++] = "-ge";
            else if(chain->ops[j].cmp == CMP_LE)  exec_argv[idx++] = "-le";
            else if(chain->ops[j].cmp == CMP_NE)  exec_argv[idx++] = "-ne";
            exec_argv[idx++] = chain->ops[j].cmp_value;
          }

          exec_argv[idx] = NULL;
          execv(exec_argv[0], exec_argv);
          perror("execv failed");
          exit(1);

        } else {
          op_pids[i][j] = pid;
        }
      }

      for(int j=0; j<num_ops-1; j++){
        close(mid_pipes[j][0]);
        close(mid_pipes[j][1]);
      }
    }
  }

  // feeder fork
  pid_t feeder_pid = fork();
  if(feeder_pid == -1){ perror("feeder fork failed"); exit(1); }

  if(feeder_pid == 0){
    for(int i=0; i<node->num_chains; i++){
      if(fd_feed[i][0] != -1)   close(fd_feed[i][0]);
      if(fd_output[i][0] != -1) close(fd_output[i][0]);
      if(fd_output[i][1] != -1) close(fd_output[i][1]);
    }
    for(int i=0; i<node->num_chains; i++){
      if(node->chains[i].merger_child != NULL) continue;
      if(fd_feed[i][1] == -1) continue;
      operator_chain_t *chain = &node->chains[i];
      for(int j=chain->start_line-1; j<chain->end_line && j<num_lines; j++){
        write(fd_feed[i][1], lines[j], strlen(lines[j]));
      }
      close(fd_feed[i][1]);
    }
    exit(0);
  } else {
    for(int i=0; i<node->num_chains; i++){
      if(node->chains[i].merger_child != NULL) continue;
      if(fd_feed[i][0] != -1){ close(fd_feed[i][0]); fd_feed[i][0] = -1; }
      if(fd_feed[i][1] != -1){ close(fd_feed[i][1]); fd_feed[i][1] = -1; }
    }
  }

  // Parent must close output write-ends, otherwise read side may block forever.
  for(int i=0; i<node->num_chains; i++){
    if(fd_output[i][1] != -1){
      close(fd_output[i][1]);
      fd_output[i][1] = -1;
    }
  }

  // output pipe'lardan sırayla oku
  for(int i=0; i<node->num_chains; i++){
    if(fd_output[i][0] == -1) continue;
    char buf[4096];
    int n;
    while((n = read(fd_output[i][0], buf, sizeof(buf))) > 0){
      write(STDOUT_FILENO, buf, n);
    }
    close(fd_output[i][0]);
    fd_output[i][0] = -1;
  }

  // waitpid loop
  pid_t pid;
  int status;
  while((pid = waitpid(-1, &status, 0)) > 0){
    for(int i=0; i<node->num_chains; i++){
      for(int j=0; j<op_counts[i]; j++){
        if(op_pids[i][j] == pid){
          if(WIFEXITED(status))
            exit_statuses[i][j] = WEXITSTATUS(status);
          else
            exit_statuses[i][j] = -1;
        }
      }
    }
  }

  // EXIT-STATUS yazdır
  for(int i=0; i<node->num_chains; i++){
    for(int j=0; j<op_counts[i]; j++){
      printf("EXIT-STATUS %d %d\n", op_pids[i][j], exit_statuses[i][j]);
    }
  }
}

int main(int argc, char **argv)
{
  merger_node_t *root = parse_merger_input(stdin);
  if(!root){
    fprintf(stderr, "merger: parse error\n");
    return 1;
  }

  int num_lines = 0;
  char **lines = NULL;

  if(root->has_filename){
    FILE *f = fopen(root->filename, "r");
    if(!f){ perror("fopen"); exit(1); }
    lines = get_lines(f, &num_lines);
    fclose(f);
  } else {
    lines = get_lines(stdin, &num_lines);
  }

  run_merger(root, lines, num_lines);

  for(int i=0; i<num_lines; i++) free(lines[i]);
  free(lines);
  free_merger_tree(root);
  return 0;
}
