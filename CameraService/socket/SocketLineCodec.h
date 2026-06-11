#ifndef SOCKETLINECODEC_H
#define SOCKETLINECODEC_H

#include <QByteArray>
#include <QString>

// 本地套接字共用的「一行一条命令」缓冲解析
class SocketLineCodec
{
public:
    // 从接收缓冲中取出一行（以 \n 分隔），并移出缓冲
    static bool takeLine(QByteArray *buffer, QByteArray *lineOut)
    {
        if (!buffer || !lineOut)
            return false;
        const int idx = buffer->indexOf('\n');
        if (idx < 0)
            return false;
        *lineOut = buffer->left(idx).trimmed();
        buffer->remove(0, idx + 1);
        return true;
    }

    // 将文本编码为 UTF-8 并追加换行符
    static QByteArray encodeLine(const QString &text)
    {
        QByteArray out = text.toUtf8();
        out.append('\n');
        return out;
    }
};

#endif // SOCKETLINECODEC_H
