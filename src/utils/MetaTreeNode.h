#ifndef METATREENODE_H
#define METATREENODE_H
#include <QList>
#include <QVariant>
#include <QSharedPointer>
#include <QScopedPointer>
#include <QMutex>
#include <QStandardItemModel>
class MetaTreeNode
{
public:
    explicit MetaTreeNode(QString key,QVariant value="",MetaTreeNode *parent=nullptr);
    virtual ~MetaTreeNode()=default;
    QSharedPointer<MetaTreeNode> addChild(QString key,QVariant value="");
    QString key();
    QVariant value();
    QList<QSharedPointer<MetaTreeNode>> childen();
private:
    QString m_key{};
    QVariant m_value{};
    QList<QSharedPointer<MetaTreeNode>> m_children{};
    MetaTreeNode *m_parent=nullptr;
    QMutex m_lock;
};

#endif // METATREENODE_H
