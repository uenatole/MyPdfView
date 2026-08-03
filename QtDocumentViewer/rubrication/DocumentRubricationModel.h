#pragma once

#include <QAbstractItemModel>
#include <optional>

#include <Document/API/DocumentRubrication.h>

class DocumentRubricationModel : public QAbstractItemModel
{
public:
    explicit DocumentRubricationModel(QObject* parent = nullptr);
    ~DocumentRubricationModel() override;

    void setRubrication(const DocumentRubrication& rubrication);
    void clear();

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    const DocumentRubric* getRubric(const QModelIndex& index) const;
    bool hasAction(const QModelIndex& index) const;
    std::optional<DocumentAction> getAction(const QModelIndex& index) const;

private:
    struct Node
    {
        std::optional<DocumentRubric> rubric;
        Node* parent = nullptr;
        QList<Node*> children;
        
        Node() = default;
        ~Node() { qDeleteAll(children); }
    };

    Node* m_rootNode = nullptr;

    void buildTree(Node* parent, const QList<DocumentRubric>& rubrics);
    Node* nodeFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromNode(Node* node, int column = 0) const;
};
