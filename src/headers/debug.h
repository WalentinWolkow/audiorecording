#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <iostream>

#define BLNK_ON         "\033[?12h"
#define BLNK_OFF        "\033[?12l"
#define SHOW_CUR        "\033[?25h"
#define HIDE_CUR        "\033[?25l"

#define RED_CLR         "\033[31m"
#define GREEN_CLR       "\033[32m"
#define YELLOW_CLR      "\033[33m"
#define BLUE_CLR        "\033[34m"
#define RES_CLR         "\033[0m"

#define HERE()          std::cout << GREEN_CLR << __FILE__ << ", " << __LINE__ << RES_CLR << std::endl
#define DBG(x)          std::cout << YELLOW_CLR << __FILE__ << ", " << __LINE__ << ": " << x << RES_CLR << std::endl
#define PRINT(x)        std::cout << x << std::endl;
#define ERR(x)          std::cout << RED_CLR << __FILE__ << ", " << __LINE__ << ": " << x << RES_CLR << std::endl

#endif  // __DEBUG_H__
