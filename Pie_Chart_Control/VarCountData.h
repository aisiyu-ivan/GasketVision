#ifndef VARCOUNTDATA_H
#define VARCOUNTDATA_H

#include <QString>

typedef struct tagVarCountData// 变量结构体
{
    QString name;// 变量名
    unsigned int count; // 变量计数
} VarCountData;

#endif // VARCOUNTDATA_H
