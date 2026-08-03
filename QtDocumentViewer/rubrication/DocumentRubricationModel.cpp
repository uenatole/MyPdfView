#include "DocumentRubricationModel.h"

#include <QColor>
#include <QFont>

DocumentRubricationModel::DocumentRubricationModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootNode(new Node())
{}

DocumentRubricationModel::~DocumentRubricationModel()
{
    delete m_rootNode;
}

void DocumentRubricationModel::setRubrication(const DocumentRubrication& rubrication)
{
    beginResetModel();

    delete m_rootNode;
    m_rootNode = new Node();

    buildTree(m_rootNode, rubrication.Children);

    endResetModel();
}

void DocumentRubricationModel::clear()
{
    beginResetModel();
    delete m_rootNode;
    m_rootNode = new Node();
    endResetModel();
}

void DocumentRubricationModel::buildTree(Node* parent, const QList<DocumentRubric>& rubrics)
{
    for (const DocumentRubric& rubric : rubrics)
    {
        Node* node = new Node();
        node->rubric = rubric;
        node->parent = parent;

        if (!rubric.Children.isEmpty()) {
            buildTree(node, rubric.Children);
        }

        parent->children.append(node);
    }
}

QModelIndex DocumentRubricationModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    Node* parentNode = nodeFromIndex(parent);
    if (!parentNode)
        return QModelIndex();

    if (row < 0 || row >= parentNode->children.size())
        return QModelIndex();

    Node* childNode = parentNode->children[row];
    return createIndex(row, column, childNode);
}

QModelIndex DocumentRubricationModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    Node* childNode = static_cast<Node*>(child.internalPointer());
    if (!childNode || !childNode->parent)
        return QModelIndex();

    Node* parentNode = childNode->parent;

    if (parentNode == m_rootNode)
        return QModelIndex();

    Node* grandParent = parentNode->parent;
    if (!grandParent)
        return QModelIndex();

    int row = grandParent->children.indexOf(parentNode);
    if (row < 0)
        return QModelIndex();

    return createIndex(row, 0, parentNode);
}

int DocumentRubricationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;

    Node* parentNode = nodeFromIndex(parent);
    if (!parentNode)
        return 0;

    return parentNode->children.size();
}

int DocumentRubricationModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant DocumentRubricationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    Node* node = static_cast<Node*>(index.internalPointer());
    if (!node || !node->rubric)
        return QVariant();

    if (role == Qt::DisplayRole) {
        return node->rubric->Title;
    }

    return QVariant();
}

Qt::ItemFlags DocumentRubricationModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    Node* node = static_cast<Node*>(index.internalPointer());
    if (node && node->rubric && node->rubric->Action.has_value()) {
        // Можно добавить специальный флаг для элементов с Action
        flags |= Qt::ItemIsSelectable;
    }

    return flags;
}

QVariant DocumentRubricationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (section == 0)
        {
            return "Document outline";
        }
    }
    return QVariant();
}

const DocumentRubric* DocumentRubricationModel::getRubric(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;

    Node* node = static_cast<Node*>(index.internalPointer());
    return node ? (node->rubric ? &*node->rubric : nullptr) : nullptr;
}

bool DocumentRubricationModel::hasAction(const QModelIndex& index) const
{
    const DocumentRubric* rubric = getRubric(index);
    return rubric && rubric->Action.has_value();
}

std::optional<DocumentAction> DocumentRubricationModel::getAction(const QModelIndex& index) const
{
    const DocumentRubric* rubric = getRubric(index);
    if (rubric && rubric->Action.has_value()) {
        return rubric->Action.value();
    }
    return std::nullopt;
}

DocumentRubricationModel::Node* DocumentRubricationModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_rootNode;

    return static_cast<Node*>(index.internalPointer());
}

QModelIndex DocumentRubricationModel::indexFromNode(Node* node, int column) const
{
    if (!node || node == m_rootNode || !node->parent)
        return QModelIndex();
    
    int row = node->parent->children.indexOf(node);
    if (row < 0)
        return QModelIndex();
    
    return createIndex(row, column, node);
}
