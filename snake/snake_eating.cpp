// Snake eating.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include<iostream>
#include<string>
#include<Windows.h>
#include<time.h>
#include<vector>

using namespace std;
#define KEY_DOWN(vk_code) ( GetAsyncKeyState(vk_code) & 0x8000 ? 1:0)

enum
{
	E_DIR_NONE,
	E_DIR_UP,
	E_DIR_DOWN,
	E_DIR_LEFT,
	E_DIR_RIGHT
};

struct STile
{
	STile()
	{

	}
	STile(int row, int col, string img)
	{
		nRow = row;
		nCol = col;
		strImg = img;
		nextNode = nullptr;
	}
	int nRow;
	int nCol;
	int nRowBk;
	int nColBk;
	string strImg;
	STile* nextNode;
};

//定义所有变量：
const int ROW = 20;
const int COL = 20;
int arrMap[ROW][COL];

//循环之前 数据初始化
int dir = E_DIR_NONE;
int nTime = 0;

STile* pHead = new STile(6, 6, "¤");
STile* pLast = pHead;

STile* food = new STile(9, 9, "☆");

//定义随机食物位置的函数
void randomFood()
{
	food = new STile(rand() % (ROW - 2) + 1, rand() % (COL - 2) + 1, "☆");
	/*food->nRow = rand() % (ROW - 2) + 1;
	food->nCol = rand() % (COL - 2) + 1;*/

	for (STile* pNode = pHead; pNode; pNode = pNode->nextNode)
	{
		//判断食物行列和蛇身体某一节是否重叠
		if (food->nRow == pNode->nRow && food->nCol == pNode->nCol)
		{
			randomFood(); //递归
		}
		break;
	}
}

void update()
{
	//遍历蛇的所有身体 并且备份
	for (STile* pNode = pHead; pNode; pNode = pNode->nextNode)
	{
		pNode->nRowBk = pNode->nRow;
		pNode->nColBk = pNode->nCol;
	}

	//上下左右键来控制方向
	if (KEY_DOWN(VK_UP))
	{
		dir = E_DIR_UP;
	}
	else if (KEY_DOWN(VK_DOWN))
	{
		dir = E_DIR_DOWN;
	}
	else if (KEY_DOWN(VK_LEFT))
	{
		dir = E_DIR_LEFT;
	}
	else if (KEY_DOWN(VK_RIGHT))
	{
		dir = E_DIR_RIGHT;
	}

	//控制蛇的移动速度
	nTime++;
	if (nTime >= 5)
	{
		nTime = 0;
		switch (dir)
		{
		case E_DIR_UP:
			pHead->nRow--;
			break;
		case E_DIR_DOWN:
			pHead->nRow++;
			break;
		case E_DIR_LEFT:
			pHead->nCol--;
			break;
		case E_DIR_RIGHT:
			pHead->nCol++;
			break;
		default:
			break;
		}

		//蛇身体跟随蛇移动
		if (dir)
		{
			for (STile* pNode = pHead; pNode->nextNode; pNode = pNode->nextNode)
			{
				pNode->nextNode->nRow = pNode->nRowBk;
				pNode->nextNode->nCol = pNode->nColBk;
			}
		}
	} //时间循环结束	

	//蛇吃食物然后增长蛇的长度 食物随机刷新
	if (pHead->nRow == food->nRow && pHead->nCol == food->nCol)
	{
		STile* pNode = pLast;
		/*for (STile* pNode = pLast; pNode; pNode = pLast->nextNode)
		{*/
		pNode->nCol = pLast->nColBk;
		pNode->nRow = pLast->nRowBk;
		/*pLast->nextNode = pNode;
		pLast = pNode;*/
		randomFood();
		//}
	}
}

void render()
{
	//渲染
	for (int i = 0; i < ROW; i++)
	{
		for (int j = 0; j < COL; j++)
		{
			if (i == 0 || j == 0 || i == ROW - 1 || j == COL - 1)
			{
				arrMap[i][j] = 1;
			}
			else
			{
				arrMap[i][j] = 0;
			}

			//用遍历的方法判断蛇的身体 然后再地图上渲染出蛇的每一节
			STile* snake = nullptr;
			for (STile* pNode = pHead; pNode; pNode = pNode->nextNode)
			{
				if (pNode->nRow == i && pNode->nCol == j)
				{
					snake = pNode;
				}
			}

			if (1 == arrMap[i][j])//墙
			{
				cout << "■";
			}
			else if (snake)
			{
				cout << snake->strImg;
			}
			else if (food->nRow == i && food->nCol == j)
			{
				cout << food->strImg;
			}
			else
			{
				cout << "  "; //填充空白
			}
		}
		cout << endl;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	/*停止画面闪烁
	HANDLE hOutput;
	COORD coord = { 0, 0 };
	hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	创建新的缓冲区
	HANDLE hOutBuf = CreateConsoleScreenBuffer(
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CONSOLE_TEXTMODE_BUFFER,
		NULL
		);

	设置新的缓冲区为活动显示缓冲
	SetConsoleActiveScreenBuffer(hOutBuf);

	隐藏两个缓冲区的光标
	CONSOLE_CURSOR_INFO cci;
	cci.bVisible = 0;
	cci.dwSize = 1;
	SetConsoleCursorInfo(hOutput, &cci);
	SetConsoleCursorInfo(hOutBuf, &cci);

	双缓冲处理显示
	DWORD bytes = 0;
	char data[3200];*/

	//使每一次重新开始游戏时 食物随机出现的位置都不固定
	srand(time(NULL));

	//用遍历构建蛇的身体
	for (int i = 0; i < 4; i++)
	{
		STile* pBody = new STile(6, 7 + i, "◎");
		pLast->nextNode = pBody;
		pLast = pBody;
	}

	while (true)
	{
		system("cls");
		update();
		render();

		////停止闪烁
		//ReadConsoleOutputCharacterA(hOutput, data, 3200, coord, &bytes);
		//WriteConsoleOutputCharacterA(hOutBuf, data, 3200, coord, &bytes);

	}//while true的结束括号 
	return 0;
}