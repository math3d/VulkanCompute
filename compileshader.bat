:: https://stackoverflow.com/questions/4036754/why-does-only-the-first-line-of-this-windows-batch-file-execute-but-all-three-li
cd data\shaders\ && ^
.\generate-spirv.bat  && ^
cd add && ^
.\generate-spirv.bat && ^
cd ..\add_vec4 && ^
.\generate-spirv.bat && ^
cd ..\add_image && ^
.\generate-spirv.bat && ^
cd ..\add_imager32f && ^
.\generate-spirv.bat && ^
cd ..\..\..\