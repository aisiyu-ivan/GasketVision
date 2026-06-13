@echo off
setlocal
cd /d "%~dp0"

set "GH=%~dp0.tools\gh\bin\gh.exe"
set "REPO_NAME=GasketVision"

echo === GasketVision - push to GitHub ===
echo.

git remote get-url origin >nul 2>&1
if errorlevel 1 (
    echo Remote 'origin' not set.
    echo.
    if exist "%GH%" (
        "%GH%" auth status >nul 2>&1
        if errorlevel 1 (
            echo [1] Log in to GitHub first:
            echo     "%GH%" auth login
            echo.
            echo [2] Create repo and push:
            echo     "%GH%" repo create %REPO_NAME% --public --source=. --remote=origin --push
            echo.
            echo Or create an empty repo on https://github.com/new then run:
            set /p GH_USER=GitHub username: 
            git remote add origin https://github.com/%GH_USER%/%REPO_NAME%.git
            git push -u origin main
            goto :eof
        )
        echo Creating GitHub repo and pushing...
        "%GH%" repo create %REPO_NAME% --public --source=. --remote=origin --push
        if errorlevel 1 exit /b 1
        echo Done.
        goto :eof
    )
    echo Install GitHub CLI or create repo at https://github.com/new
    set /p GH_USER=GitHub username: 
    git remote add origin https://github.com/%GH_USER%/%REPO_NAME%.git
)

echo Pushing to origin main...
git push -u origin main
if errorlevel 1 (
    echo.
    echo Push failed. Create empty repo at https://github.com/new ^(no README^)
    echo then run this script again.
    exit /b 1
)
echo Done: https://github.com/%REPO_NAME%
endlocal
