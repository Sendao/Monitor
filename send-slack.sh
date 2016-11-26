#!/bin/bash

# Will post a message to AlphaSheets Slack
# First arg: message, second arg: channel (#general, @riteshr), third arg: bot name 

#WEBHOOK_URL="https://hooks.slack.com/services/keys
CHANNEL=${2:-"#general"}
USERNAME=${3:-"as-bot"}
TEXT=`cat ${1:-"xx"}`

#echo test: $TEXT
#echo test2: $USERNAME
#echo test3: $CHANNEL

# Note that we don't currently do escaping, since we want newlines to show up
# Note that there are some weird character transforms that slack expects with &, @ etc that we're not taking care of right now

JSON="{\"channel\": \"$CHANNEL\", \"username\":\"$USERNAME\", \"icon_emoji\":\"ghost\", \"attachments\":[{\"color\":\"#764FA5\" , \"text\": \"$TEXT\", \"mrkdwn_in\": [\"text\"]}]}"

# The -k is sort of a hack, couldn't get certificate stuff to work out
curl -k -d "payload=$JSON" "$WEBHOOK_URL"

echo $JSON >> slack_logs.txt
