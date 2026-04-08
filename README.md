[새 브랜치 만들기 및 이동]
git checkout -b feature-test
파일 수정 및 저장: ctrl + s
변경사항 저장
    git add .
    git commit -m "PR 테스트를 위한 내용 추가"
새 브랜치로 푸시
    git push origin feature-test

[main에 올리는 방법]
파일 수정하기
파일 저장하기: ctrl + s
Git 명령어 입력하기
    git add .
    git commit -m "README 파일 테스트 수정"
    git push origin main