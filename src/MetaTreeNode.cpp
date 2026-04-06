#include "MetaTreeNode.h"

MetaTreeNode::MetaTreeNode(QString key, QVariant value,MetaTreeNode *parent)
    :m_key{key},m_value{value},m_parent{parent}
{

}

QSharedPointer<MetaTreeNode> MetaTreeNode::addChild(QString key, QVariant value)
{
    QMutexLocker locker(&m_lock);
    auto child=QSharedPointer<MetaTreeNode>::create(key,value,this);
    m_children.push_back(child);
    return child;
}

QString MetaTreeNode::key()
{
    QMutexLocker locker(&m_lock);
    return m_key;
}

QVariant MetaTreeNode::value()
{
    QMutexLocker locker(&m_lock);
    return m_value;
}

QList<QSharedPointer<MetaTreeNode>> MetaTreeNode::childen()
{
    QMutexLocker locker(&m_lock);
    return m_children;
}
