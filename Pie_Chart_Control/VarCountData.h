#ifndef VARCOUNTDATA_H
#define VARCOUNTDATA_H

#include <QString>

// 饼图扇区数据：变量名及其计数
typedef struct tagVarCountData
{
    QString name;           // 变量名
    unsigned int count = 0; // 变量计数
} VarCountData;

#endif // VARCOUNTDATA_H
