if [[ $# < 2 ]]
    then
        echo "usage: $(basename $0) \"output to match against\" command <optional args>"
fi

string="$1"
shift 1
cmd="$@"

it=1

while true
do
        rm UNTILLOUTPUTMATCHESEOF.txt > /dev/null 2>&1
        script --flush --quiet --command "$cmd" UNTILLOUTPUTMATCHESEOF.txt
        grep -q "$string" UNTILLOUTPUTMATCHESEOF.txt
        if [[ $? == 0 ]]
            then
            cat UNTILLOUTPUTMATCHESEOF.txt
            rm UNTILLOUTPUTMATCHESEOF.txt
            if [[ $it == 1 ]]
                then
                    echo "found in 1 try"
                else
                    echo "found in $it tries"
            fi
            exit
        fi
        it=$(($it + 1))
done
