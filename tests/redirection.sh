# test_redirection.sh — input and output redirection
echo this is output redirection > /tmp/mysh_out.txt
cat /tmp/mysh_out.txt
echo overwritten > /tmp/mysh_out.txt
cat /tmp/mysh_out.txt
cat < /tmp/mysh_out.txt
echo both directions > /tmp/mysh_copy.txt
cat < /tmp/mysh_copy.txt > /tmp/mysh_copy2.txt
cat /tmp/mysh_copy2.txt
pwd > /tmp/mysh_pwd.txt
cat /tmp/mysh_pwd.txt
