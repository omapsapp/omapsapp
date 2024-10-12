#pragma once

#include "map/framework.hpp"

#include "platform/downloader_defines.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

class Framework;

namespace qt
{
  class UpdateDialog : public QDialog
  {
    Q_OBJECT

  public:
    explicit UpdateDialog(QWidget * parent, Framework & framework);
    virtual ~UpdateDialog();

    /// @name Called from downloader to notify GUI
    //@{
    void OnCountryChanged(storage::CountryId const & countryId);
    void OnCountryDownloadProgress(storage::CountryId const & countryId,
                                   downloader::Progress const & progress);
    //@}

    void ShowModal();

  private slots:
    void OnItemClick(QTreeWidgetItem * item, int column);
    void OnCloseClick();
    void OnLocaleTextChanged(QString const & text);
    void OnQueryTextChanged(QString const & text);

  private:
    // CountryId to its ranking position and matched string (assuming no duplicates).
    using Filter = std::unordered_map<storage::CountryId, std::pair<size_t, std::string>>;

    void RefillTree();
    void StartSearchInDownloader();

    // Adds only those countries present in |filter|.
    // Calls whose timestamp is not the latest are discarded.
    void FillTree(std::optional<Filter> const & filter, uint64_t timestamp);
    void FillTreeImpl(QTreeWidgetItem * parent, storage::CountryId const & countryId,
                      std::optional<Filter> const & filter);

    void UpdateRowWithCountryInfo(storage::CountryId const & countryId);
    void UpdateRowWithCountryInfo(QTreeWidgetItem * item, storage::CountryId const & countryId);
    QString GetNodeName(storage::CountryId const & countryId);

    QTreeWidgetItem * CreateTreeItem(storage::CountryId const & countryId, size_t posInRanking,
                                     std::string matchedBy, QTreeWidgetItem * parent);
    std::vector<QTreeWidgetItem *> GetTreeItemsByCountryId(storage::CountryId const & countryId);
    storage::CountryId GetCountryIdByTreeItem(QTreeWidgetItem *);

    inline storage::Storage & GetStorage() const { return m_framework.GetStorage(); }

    QTreeWidget * m_tree;
    Framework & m_framework;
    int m_observerSlotId;

    // Params of the queries to the search engine.
    std::string m_query;
    std::string m_locale = "en";
    uint64_t m_fillTreeTimestamp = 0;

    std::unordered_multimap<storage::CountryId, QTreeWidgetItem *> m_treeItemByCountryId;
  };
}  // namespace qt
