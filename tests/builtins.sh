# test_builtins.sh — all built-in commands
pwd
cd /tmp
pwd
cd
pwd
which ls
which echo
which grep
which exit
which pwd
which cd
which which
pwd > /tmp/mysh_builtin_out.txt
cat /tmp/mysh_builtin_out.txt
which ls > /tmp/mysh_which_out.txt
cat /tmp/mysh_which_out.txt
exit
echo this should never print
