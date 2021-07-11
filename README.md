This repo contains the code for my YouTube class:
"Bare Metal Embedded Software Development: Theory and Practice Using STM32".
This course consists of around 25 video lessons. The URL is:

https://www.youtube.com/playlist?list=PL4cGeWgaBTe155QQSQ72DksLIjBn5Jn2Z

This course was developed using a `ST NUCLEO-F401RE` board. Since then, I ported
the code to run on a `ST NUCLEO-L452RE` board. That code is on branch
`nucleo-l452re`. Someday I might merge these two branches, and use `#defines` to
select the board type.

If you do use the branch for STM32LXX, be aware there is a bug in the IDE for
this case. I reported it here:

https://community.st.com/s/profile/0053W000001pgpwQAA

To workaround this bug, you have to change this generated line of code in `main.c`:

`LL_RCC_HSI_SetCalibTrimming(16);`

to:

`LL_RCC_HSI_SetCalibTrimming(64);`
