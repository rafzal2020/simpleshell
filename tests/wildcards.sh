# test_wildcards.sh — wildcard expansion
# setup: create some temp files to match against
cd /tmp
echo a > fooabar.txt
echo b > foobbar.txt
echo c > foocbar.txt
echo test > other.txt
echo foo*bar.txt
echo *.txt
echo foo*baz
echo *bar.txt
cd
