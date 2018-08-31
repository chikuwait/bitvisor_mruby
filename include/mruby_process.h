int create_mruby_process();
int load_mruby_process(int mruby_process);
int mruby_funcall(int mruby_process, char *str, int argc, ...);
int exit_mruby_process(int mruby_process);
