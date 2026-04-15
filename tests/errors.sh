# test_errors.sh — error handling, shell must continue after each error
notarealcommand
cd doesnotexist
cd foo bar baz
echo hello
which
which cd
which doesnotexist
echo still running after errors
