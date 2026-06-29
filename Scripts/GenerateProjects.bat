@echo off

pushd ..
git submodule sync --recursive
git submodule update --init --recursive --force
ThirdParty\premake\premake5.exe vs2022
popd
pause