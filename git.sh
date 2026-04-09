#!/bin/bash

current_date=$(date +%Y-%m-%d)

commit_message="Update $current_date"

# Git 操作
git add .
git status
git commit -m "$commit_message"
git push

echo "提交完成: $commit_message"