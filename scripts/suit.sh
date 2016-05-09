# This file must be used with "source bin/activate" *from bash*
# you cannot run it directly

deactivate () {
    unset SUIT_HOME 
    unset SUIT_PASSPHRASE
    unset SUIT_NAME
    unset SUIT_STATUSMSG
    unset SUIT_LEVEL
    unset suitinfo
}

suitinfo () {
	toxdatatool -p $SUIT_HOME/data.tox -s $SUIT_PASSPHRASE
}

suitenv () {
	echo SUIT_HOME : $SUIT_HOME
    echo SUIT_PASSPHRASE : $SUIT_PASSPHRASE
    echo SUIT_NAME : $SUIT_NAME
    echo SUIT_STATUSMSG : $SUIT_STATUSMSG
    echo SUIT_LEVEL : $SUIT_LEVEL
}

alias suittop="htop -p $(pidof suit|tr ' ' ',')"
alias suitkill="pidof suit | xargs -r kill -SIGINT"

# dont provide trailing / on path
declare -x SUIT_HOME="/home/genesis/suit"
declare -x SUIT_PASSPHRASE="somethingintheway"
declare -x SUIT_NAME="Anna"
declare -x SUIT_STATUSMSG="A Tox service bot based on ToxSuite."
declare -x SUIT_LEVEL=5
