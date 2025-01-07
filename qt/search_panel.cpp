#include "qt/search_panel.hpp"
#include "qt/draw_widget.hpp"

#include "search/search_params.hpp"

#include "map/bookmark_manager.hpp"
#include "map/framework.hpp"

#include "base/assert.hpp"

#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

namespace qt
{
SearchPanel::SearchPanel(DrawWidget * drawWidget, QWidget * parent)
  : QWidget(parent)
  , m_pDrawWidget(drawWidget)
  , m_clearIcon(":/ui/x.png")
  , m_busyIcon(":/ui/busy.png")
  , m_mode(search::Mode::Everywhere)
  , m_timestamp(0)
{
  m_pEditor = new QLineEdit(this);
  connect(m_pEditor, &QLineEdit::textChanged, this, &SearchPanel::OnSearchTextChanged);

  m_pTable = new QTableWidget(0, 4 /*columns*/, this);
  m_pTable->setFocusPolicy(Qt::NoFocus);
  m_pTable->setAlternatingRowColors(true);
  m_pTable->setShowGrid(false);
  m_pTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_pTable->verticalHeader()->setVisible(false);
  m_pTable->horizontalHeader()->setVisible(false);
  m_pTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  connect(m_pTable, &QTableWidget::cellClicked, this, &SearchPanel::OnSearchPanelItemClicked);

  m_pClearButton = new QPushButton(this);
  connect(m_pClearButton, &QAbstractButton::pressed, this, &SearchPanel::OnClearButton);
  m_pClearButton->setVisible(false);
  m_pClearButton->setFocusPolicy(Qt::NoFocus);
  m_pAnimationTimer = new QTimer(this);
  connect(m_pAnimationTimer, &QTimer::timeout, this, &SearchPanel::OnAnimationTimer);

  QButtonGroup * searchModeButtons = new QButtonGroup(this);
  QGroupBox * groupBox = new QGroupBox();
  QHBoxLayout * modeLayout = new QHBoxLayout();
  QRadioButton * buttonE = new QRadioButton(tr("Everywhere"));
  modeLayout->addWidget(buttonE);
  searchModeButtons->addButton(buttonE, static_cast<int>(search::Mode::Everywhere));
  QRadioButton * buttonV = new QRadioButton(tr("Viewport"));
  modeLayout->addWidget(buttonV);
  searchModeButtons->addButton(buttonV, static_cast<int>(search::Mode::Viewport));
  groupBox->setLayout(modeLayout);
  groupBox->setFlat(true);
  searchModeButtons->button(static_cast<int>(search::Mode::Everywhere))->setChecked(true);
  connect(searchModeButtons, SIGNAL(idClicked(int)), this, SLOT(OnSearchModeChanged(int)));

  m_isCategory = new QCheckBox(tr("Category request"));
  m_isCategory->setCheckState(Qt::Unchecked);
  connect(m_isCategory, &QCheckBox::stateChanged, std::bind(&SearchPanel::RunSearch, this));

  QHBoxLayout * requestLayout = new QHBoxLayout();
  requestLayout->addWidget(m_pEditor);
  requestLayout->addWidget(m_pClearButton);
  QVBoxLayout * verticalLayout = new QVBoxLayout();
  verticalLayout->addWidget(groupBox);
  verticalLayout->addWidget(m_isCategory);
  verticalLayout->addLayout(requestLayout);
  verticalLayout->addWidget(m_pTable);
  setLayout(verticalLayout);
}

namespace
{
QTableWidgetItem * CreateItem(std::string const & s)
{
  QTableWidgetItem * item = new QTableWidgetItem(QString::fromStdString(s));
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  return item;
}
}  // namespace

void SearchPanel::ClearTable()
{
  m_pTable->clear();
  m_pTable->setRowCount(0);
}

void SearchPanel::ClearResults()
{
  ClearTable();
  m_results.Clear();
  GetFramework().GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::SEARCH);
}

void SearchPanel::StartBusyIndicator()
{
  if (!m_pAnimationTimer->isActive())
    m_pAnimationTimer->start(200 /* milliseconds */);

  m_pClearButton->setFlat(true);
  m_pClearButton->setVisible(true);
}

void SearchPanel::StopBusyIndicator()
{
  m_pAnimationTimer->stop();
  m_pClearButton->setIcon(m_clearIcon);
}

void SearchPanel::OnEverywhereSearchResults(uint64_t timestamp, search::Results results)
{
  CHECK(m_threadChecker.CalledOnOriginalThread(), ());

  CHECK_LESS_OR_EQUAL(timestamp, m_timestamp, ());
  if (timestamp != m_timestamp)
    return;

  m_results = std::move(results);
  ClearTable();

  for (auto const & res : m_results)
  {
    QString const name = QString::fromStdString(res.GetString());
    QString strHigh;
    int pos = 0;
    for (size_t r = 0; r < res.GetHighlightRangesCount(); ++r)
    {
      std::pair<uint16_t, uint16_t> const & range = res.GetHighlightRange(r);
      strHigh.append(name.mid(pos, range.first - pos));
      strHigh.append("<font color=\"green\">");
      strHigh.append(name.mid(range.first, range.second));
      strHigh.append("</font>");

      pos = range.first + range.second;
    }
    strHigh.append(name.mid(pos));

    int const rowCount = m_pTable->rowCount();
    m_pTable->insertRow(rowCount);
    m_pTable->setCellWidget(rowCount, 1, new QLabel(strHigh));
    m_pTable->setItem(rowCount, 2, CreateItem(res.GetAddress()));

    bool showDistance = true;
    switch (res.GetResultType())
    {
    case search::Result::Type::SuggestFromFeature:
    case search::Result::Type::PureSuggest:
      showDistance = false;
      break;
    case search::Result::Type::Feature:
    case search::Result::Type::Postcode:
      m_pTable->setItem(rowCount, 0, CreateItem(res.GetLocalizedFeatureType()));
      break;
    case search::Result::Type::LatLon:
      m_pTable->setItem(rowCount, 0, CreateItem("LatLon"));
      break;
    }

    if (showDistance)
      m_pTable->setItem(rowCount, 3, CreateItem(m_pDrawWidget->GetDistance(res)));
  }

  GetFramework().FillSearchResultsMarks(true /* clear */, m_results);

  if (m_results.IsEndMarker())
    StopBusyIndicator();
}

bool SearchPanel::Try3dModeCmd(std::string const & str)
{
  bool const is3dModeOn = (str == "?3d");
  bool const is3dBuildingsOn = (str == "?b3d");
  bool const is3dModeOff = (str == "?2d");

  if (!is3dModeOn && !is3dBuildingsOn && !is3dModeOff)
    return false;

  GetFramework().Allow3dMode(false, is3dModeOn, is3dBuildingsOn);

  return true;
}

bool SearchPanel::TryTrafficSimplifiedColorsCmd(std::string const & str)
{
  bool const simplifiedMode = (str == "?tc:simp");
  bool const normalMode = (str == "?tc:norm");

  if (!simplifiedMode && !normalMode)
    return false;

  bool const isSimplified = simplifiedMode;
  GetFramework().GetTrafficManager().SetSimplifiedColorScheme(isSimplified);
  GetFramework().SaveTrafficSimplifiedColors(isSimplified);

  return true;
}

std::string SearchPanel::GetCurrentInputLocale()
{
  /// @DebugNote
  // Hardcode search input language.
  //return "de";

  QString loc = QGuiApplication::inputMethod()->locale().name();
  loc.replace('_', '-');
  auto res = loc.toStdString();
  if (CategoriesHolder::MapLocaleToInteger(res) < 0)
    res = "en";
  return res;
}

void SearchPanel::OnSearchTextChanged(QString const & str)
{
  // Pass input query as-is without any normalization.
  // Otherwise № -> No, and it's unexpectable for the search index.
  //QString const normalized = str.normalized(QString::NormalizationForm_KC);
  std::string const normalized = str.toStdString();

  if (Try3dModeCmd(normalized))
    return;
  if (TryTrafficSimplifiedColorsCmd(normalized))
    return;

  ClearResults();

  if (normalized.empty())
  {
    GetFramework().GetSearchAPI().CancelAllSearches();

    // hide X button
    m_pClearButton->setVisible(false);
    return;
  }

  bool const isCategory = m_isCategory->isChecked();
  auto const timestamp = ++m_timestamp;

  using namespace search;
  if (m_mode == Mode::Everywhere)
  {
    EverywhereSearchParams params
    {
      normalized, GetCurrentInputLocale(), {} /* timeout */, isCategory,
      // m_onResults
      [this, timestamp](Results results, std::vector<ProductInfo> /* productInfo */)
      {
        OnEverywhereSearchResults(timestamp, std::move(results));
      }
    };

    if (GetFramework().GetSearchAPI().SearchEverywhere(std::move(params)))
      StartBusyIndicator();
  }
  else if (m_mode == Mode::Viewport)
  {
    ViewportSearchParams params
    {
      normalized, GetCurrentInputLocale(), {} /* timeout */, isCategory,
      // m_onStarted
      [this]()
      {
        StartBusyIndicator();
      },
      // m_onCompleted
      [this](search::Results results)
      {
        // |m_pTable| is not updated here because the OnResults callback is recreated within
        // SearchAPI when the viewport is changed. Thus a single call to SearchInViewport may
        // initiate an arbitrary amount of actual search requests with different viewports, and
        // clearing the table would require additional care (or, most likely, we would need a better
        // API). This is similar to the Android and iOS clients where we do not show the list of
        // results in the viewport search mode.
        GetFramework().FillSearchResultsMarks(true /* clear */, results);
        StopBusyIndicator();
      }
    };

    GetFramework().GetSearchAPI().SearchInViewport(std::move(params));
  }
}

void SearchPanel::OnSearchModeChanged(int mode)
{
  auto const newMode = static_cast<search::Mode>(mode);
  switch (newMode)
  {
  case search::Mode::Everywhere:
  case search::Mode::Viewport:
    break;
  default:
    UNREACHABLE();
  }

  if (m_mode == newMode)
    return;
  m_mode = newMode;

  RunSearch();
}

void SearchPanel::RunSearch()
{
  auto const text = m_pEditor->text();
  m_pEditor->setText(QString());
  m_pEditor->setText(text);
}

void SearchPanel::OnSearchPanelItemClicked(int row, int)
{
  ASSERT_EQUAL(m_results.GetCount(), static_cast<size_t>(m_pTable->rowCount()), ());

  if (m_results[row].IsSuggest())
  {
    // insert suggestion into the search bar
    std::string const suggestion = m_results[row].GetSuggestionString();
    m_pEditor->setText(QString::fromUtf8(suggestion.c_str()));
  }
  else
  {
    // center viewport on clicked item
    GetFramework().ShowSearchResult(m_results[row]);
  }
}

void SearchPanel::hideEvent(QHideEvent *)
{
  GetFramework().GetSearchAPI().CancelSearch(search::Mode::Everywhere);
}

void SearchPanel::OnAnimationTimer()
{
  static int angle = 0;

  QTransform transform;
  angle += 15;
  if (angle >= 360)
    angle = 0;
  transform.rotate(angle);

  m_pClearButton->setIcon(QIcon(m_busyIcon.transformed(transform)));
}

void SearchPanel::OnClearButton()
{
  m_pEditor->setText("");
}

Framework & SearchPanel::GetFramework() const
{
  return m_pDrawWidget->GetFramework();
}

}  // namespace qt
