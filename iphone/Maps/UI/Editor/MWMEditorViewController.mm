#import "MWMEditorViewController.h"
#import "MWMAlertViewController.h"
#import "MWMAuthorizationCommon.h"
#import "MWMButtonCell.h"
#import "MWMCuisineEditorViewController.h"
#import "MWMEditorAddAdditionalNameTableViewCell.h"
#import "MWMEditorAdditionalNameTableViewCell.h"
#import "MWMEditorAdditionalNamesHeader.h"
#import "MWMEditorAdditionalNamesTableViewController.h"
#import "MWMEditorCategoryCell.h"
#import "MWMEditorCellType.h"
#import "MWMEditorNotesFooter.h"
#import "MWMEditorSelectTableViewCell.h"
#import "MWMEditorSwitchTableViewCell.h"
#import "MWMEditorSegmentedTableViewCell.hpp"
#import "MWMEditorTextTableViewCell.h"
#import "MWMNoteCell.h"
#import "MWMObjectsCategorySelectorController.h"
#import "MWMOpeningHoursEditorViewController.h"
#import "MWMPlacePageOpeningHoursCell.h"
#import "MWMStreetEditorViewController.h"
#import "MapViewController.h"
#import "SwiftBridge.h"

#import <CoreApi/Framework.h>
#import <CoreApi/StringUtils.h>

#include "platform/localization.hpp"
#include "indexer/validate_and_format_contacts.hpp"

namespace
{
NSString * const kAdditionalNamesEditorSegue = @"Editor2AdditionalNamesEditorSegue";
NSString * const kOpeningHoursEditorSegue = @"Editor2OpeningHoursEditorSegue";
NSString * const kCuisineEditorSegue = @"Editor2CuisineEditorSegue";
NSString * const kStreetEditorSegue = @"Editor2StreetEditorSegue";
NSString * const kCategoryEditorSegue = @"Editor2CategoryEditorSegue";

NSString * const kUDEditorPersonalInfoWarninWasShown = @"PersonalInfoWarningAlertWasShown";

CGFloat constexpr kDefaultHeaderHeight = 28.;
CGFloat constexpr kDefaultFooterHeight = 32.;

typedef NS_ENUM(NSUInteger, MWMEditorSection) {
  MWMEditorSectionCategory,
  MWMEditorSectionAdditionalNames,
  MWMEditorSectionAddress,
  MWMEditorSectionDetails,
  MWMEditorSectionNote,
  MWMEditorSectionButton
};

std::vector<MWMEditorCellID> const kSectionCategoryCellTypes { MWMEditorCellTypeCategory };
std::vector<MWMEditorCellID> const kSectionAddressCellTypes {
    MWMEditorCellTypeStreet, MWMEditorCellTypeBuilding, MetadataID::FMD_POSTCODE
};

std::vector<MWMEditorCellID> const kSectionNoteCellTypes { MWMEditorCellTypeNote };
std::vector<MWMEditorCellID> const kSectionButtonCellTypes { MWMEditorCellTypeReportButton };

std::map<MWMEditorCellID, Class> const kCellType2Class {
    {MWMEditorCellTypeCategory, [MWMEditorCategoryCell class]},
    {MWMEditorCellTypeAdditionalName, [MWMEditorAdditionalNameTableViewCell class]},
    {MWMEditorCellTypeAddAdditionalName, [MWMEditorAddAdditionalNameTableViewCell class]},
    {MWMEditorCellTypeAddAdditionalNamePlaceholder, [MWMEditorAdditionalNamePlaceholderTableViewCell class]},
    {MWMEditorCellTypeStreet, [MWMEditorSelectTableViewCell class]},
    {MetadataID::FMD_OPEN_HOURS, [MWMPlacePageOpeningHoursCell class]},
    {MetadataID::FMD_CUISINE, [MWMEditorSelectTableViewCell class]},
    {MetadataID::FMD_INTERNET, [MWMEditorSwitchTableViewCell class]},
    {MetadataID::FMD_DRIVE_THROUGH, [MWMEditorSegmentedTableViewCell class]},
    {MetadataID::FMD_SELF_SERVICE, [MWMEditorSegmentedTableViewCell class]},
    {MetadataID::FMD_OUTDOOR_SEATING, [MWMEditorSegmentedTableViewCell class]},
    {MWMEditorCellTypeNote, [MWMNoteCell class]},
    {MWMEditorCellTypeReportButton, [MWMButtonCell class]}
};
// Default class, if no entry in kCellType2Class.
Class kDefaultCellTypeClass = [MWMEditorTextTableViewCell class];
/// @return kDefaultCellTypeClass if cellType not specified in kCellType2Class.
Class cellClass(MWMEditorCellID cellType)
{
  auto const it = kCellType2Class.find(cellType);
  if (it == kCellType2Class.end())
    return kDefaultCellTypeClass;
  return it->second;
}

void cleanupAdditionalLanguages(std::vector<osm::LocalizedName> const & names,
                                std::vector<NSInteger> & newAdditionalLanguages)
{
  base::EraseIf(newAdditionalLanguages, [&names](NSInteger x)
                {
                  auto it = find_if(names.begin(), names.end(),
                                    [x](osm::LocalizedName const & name) { return name.m_code == x; });
                  return it != names.end();
                });
}

std::vector<MWMEditorCellID> cellsForAdditionalNames(osm::NamesDataSource const & ds,
                                                  std::vector<NSInteger> const & newAdditionalLanguages,
                                                  BOOL showAdditionalNames)
{
  std::vector<MWMEditorCellID> res;
  auto const allNamesSize = ds.names.size() + newAdditionalLanguages.size();
  if (allNamesSize != 0)
  {
    if (showAdditionalNames)
    {
      res.insert(res.begin(), allNamesSize, MWMEditorCellTypeAdditionalName);
    }
    else
    {
      auto const mandatoryNamesCount = ds.mandatoryNamesCount;
      res.insert(res.begin(), mandatoryNamesCount, MWMEditorCellTypeAdditionalName);
      if (allNamesSize > mandatoryNamesCount)
        res.push_back(MWMEditorCellTypeAddAdditionalNamePlaceholder);
    }
  }
  res.push_back(MWMEditorCellTypeAddAdditionalName);
  return res;
}

void registerCellsForTableView(std::vector<MWMEditorCellID> const & cells, UITableView * tv)
{
  for (auto const c : cells)
    [tv registerNibWithCellClass:cellClass(c)];
}
}  // namespace

@interface MWMEditorViewController ()<
    UITableViewDelegate, UITableViewDataSource, UITextFieldDelegate, MWMOpeningHoursEditorProtocol,
    MWMPlacePageOpeningHoursCellProtocol, MWMEditorCellProtocol, MWMCuisineEditorProtocol,
    MWMStreetEditorProtocol, MWMObjectsCategorySelectorDelegate, MWMNoteCellDelegate,
    MWMEditorAdditionalName, MWMButtonCellDelegate, MWMEditorAdditionalNamesProtocol>

@property(nonatomic) NSMutableDictionary<Class, UITableViewCell *> * offscreenCells;
@property(nonatomic) NSMutableArray<NSIndexPath *> * invalidCells;
@property(nonatomic) MWMEditorAdditionalNamesHeader * additionalNamesHeader;
@property(nonatomic) MWMEditorNotesFooter * notesFooter;
@property(copy, nonatomic) NSString * note;
@property(nonatomic) FeatureStatus featureStatus;
@property(nonatomic) BOOL isFeatureUploaded;

@property(nonatomic) BOOL showAdditionalNames;

@end

@implementation MWMEditorViewController
{
  std::vector<MWMEditorSection> m_sections;
  std::map<MWMEditorSection, std::vector<MWMEditorCellID>> m_cells;
  osm::EditableMapObject m_mapObject;
  std::vector<NSInteger> m_newAdditionalLanguages;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  [self configTable];
  [self configNavBar];
  auto const & fid = m_mapObject.GetID();
  self.featureStatus = osm::Editor::Instance().GetFeatureStatus(fid.m_mwmId, fid.m_index);
  self.isFeatureUploaded = osm::Editor::Instance().IsFeatureUploaded(fid.m_mwmId, fid.m_index);
  m_newAdditionalLanguages.clear();
  if (self.isCreating)
  {
    self.navigationItem.leftBarButtonItem =
    [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                                  target:self
                                                  action:@selector(onCancel)];
  }
}

- (void)setFeatureToEdit:(FeatureID const &)fid
{
  if (!GetFramework().GetEditableMapObject(fid, m_mapObject))
    NSAssert(false, @"Incorrect featureID.");
}

- (void)setEditableMapObject:(osm::EditableMapObject const &)emo
{
  NSAssert(self.isCreating, @"We should pass featureID to editor if we just editing");
  m_mapObject = emo;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [self.tableView reloadData];
}

#pragma mark - Configuration

- (void)configNavBar
{
  self.title =
      L(self.isCreating ? @"editor_add_place_title" : @"editor_edit_place_title");
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                                                    target:self
                                                    action:@selector(onSave)];
}

- (void)onCancel
{
  [self.navigationController popToRootViewControllerAnimated:YES];
}

#pragma mark - Actions

- (void)onSave
{
  if (![self.view endEditing:YES])
  {
    NSAssert(false, @"We can't save map object because one of text fields can't apply it's text!");
    return;
  }

  if (self.invalidCells.count)
  {
    NSIndexPath * ip = self.invalidCells.firstObject;
    MWMEditorTextTableViewCell * cell = [self.tableView cellForRowAtIndexPath:ip];
    [cell.textField becomeFirstResponder];
    return;
  }

  if ([self showPersonalInfoWarningAlertIfNeeded])
    return;

  auto & f = GetFramework();
  BOOL const haveNote = self.note.length;

  if (haveNote)
    f.CreateNote(m_mapObject, osm::Editor::NoteProblemType::General, self.note.UTF8String);

  switch (f.SaveEditedMapObject(m_mapObject))
  {
  case osm::Editor::SaveResult::NoUnderlyingMapError:
  case osm::Editor::SaveResult::SavingError:
    [self.navigationController popToRootViewControllerAnimated:YES];
    break;
  case osm::Editor::SaveResult::NothingWasChanged:
    [self.navigationController popToRootViewControllerAnimated:YES];
    if (haveNote)
      [self showNotesQueuedToast];
    break;
  case osm::Editor::SaveResult::SavedSuccessfully:
    osm_auth_ios::AuthorizationSetNeedCheck(YES);
    f.UpdatePlacePageInfoForCurrentSelection();
    [self.navigationController popToRootViewControllerAnimated:YES];
    break;
  case osm::Editor::SaveResult::NoFreeSpaceError:
    [self.alertController presentNotEnoughSpaceAlert];
    break;
  }
}

- (void)showNotesQueuedToast
{
  [[MWMToast toastWithText:L(@"editor_edits_sent_message")] show];
}

#pragma mark - Headers

- (MWMEditorAdditionalNamesHeader *)additionalNamesHeader
{
  if (!_additionalNamesHeader)
  {
    __weak auto weakSelf = self;
    _additionalNamesHeader = [MWMEditorAdditionalNamesHeader header:^{
      __strong auto self = weakSelf;
      self.showAdditionalNames = !self.showAdditionalNames;
    }];
  }
  return _additionalNamesHeader;
}

#pragma mark - Footers

- (MWMEditorNotesFooter *)notesFooter
{
  if (!_notesFooter)
    _notesFooter = [MWMEditorNotesFooter footerForController:self];
  return _notesFooter;
}

#pragma mark - Properties

- (void)setShowAdditionalNames:(BOOL)showAdditionalNames
{
  _showAdditionalNames = showAdditionalNames;
  [self.additionalNamesHeader setShowAdditionalNames:showAdditionalNames];
  [self configTable];
  auto const additionalNamesSectionIt =
      find(m_sections.begin(), m_sections.end(), MWMEditorSectionAdditionalNames);
  if (additionalNamesSectionIt == m_sections.end())
  {
    [self.tableView reloadData];
  }
  else
  {
    auto const sectionIndex = distance(m_sections.begin(), additionalNamesSectionIt);
    [self.tableView reloadSections:[[NSIndexSet alloc] initWithIndex:sectionIndex]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

#pragma mark - Offscreen cells

- (UITableViewCell *)offscreenCellForClass:(Class)cls
{
  auto cell = self.offscreenCells[cls];
  if (!cell)
  {
    cell = [NSBundle.mainBundle loadWithViewClass:cls owner:nil options:nil].firstObject;
    self.offscreenCells[(id<NSCopying>)cls] = cell;
  }
  return cell;
}

- (void)configTable
{
  self.offscreenCells = [NSMutableDictionary dictionary];
  self.invalidCells = [NSMutableArray array];
  m_sections.clear();
  m_cells.clear();

  m_sections.push_back(MWMEditorSectionCategory);
  m_cells[MWMEditorSectionCategory] = kSectionCategoryCellTypes;
  registerCellsForTableView(kSectionCategoryCellTypes, self.tableView);
  BOOL const isNameEditable = m_mapObject.IsNameEditable();
  BOOL const isAddressEditable = m_mapObject.IsAddressEditable();
  auto editableProperties = m_mapObject.GetEditableProperties();
  // Remove fields that are already displayed in the Address section.
  editableProperties.erase(std::remove_if(editableProperties.begin(), editableProperties.end(), [](osm::MapObject::MetadataID mid)
  {
    return mid == MetadataID::FMD_POSTCODE || mid == MetadataID::FMD_BUILDING_LEVELS;
  }), editableProperties.end());
  BOOL const isCreating = self.isCreating;
  BOOL const showNotesToOSMEditors = YES;

  if (isNameEditable)
  {
    auto const ds = m_mapObject.GetNamesDataSource();
    auto const & localizedNames = ds.names;
    cleanupAdditionalLanguages(localizedNames, m_newAdditionalLanguages);
    auto const cells =
        cellsForAdditionalNames(ds, m_newAdditionalLanguages, self.showAdditionalNames);
    m_sections.push_back(MWMEditorSectionAdditionalNames);
    m_cells[MWMEditorSectionAdditionalNames] = cells;
    registerCellsForTableView(cells, self.tableView);
    [self.additionalNamesHeader setAdditionalNamesVisible:cells.size() > ds.mandatoryNamesCount + 1];
  }

  if (isAddressEditable)
  {
    m_sections.push_back(MWMEditorSectionAddress);
    m_cells[MWMEditorSectionAddress] = kSectionAddressCellTypes;
    if (m_mapObject.IsBuilding() && !m_mapObject.IsPointType())
      m_cells[MWMEditorSectionAddress].push_back(MetadataID::FMD_BUILDING_LEVELS);

    registerCellsForTableView(kSectionAddressCellTypes, self.tableView);
  }

  if (!editableProperties.empty())
  {
    m_sections.push_back(MWMEditorSectionDetails);
    auto & v = m_cells[MWMEditorSectionDetails];
    v.assign(editableProperties.begin(), editableProperties.end());
    registerCellsForTableView(v, self.tableView);
  }

  if (showNotesToOSMEditors)
  {
    m_sections.push_back(MWMEditorSectionNote);
    m_cells[MWMEditorSectionNote] = kSectionNoteCellTypes;
    registerCellsForTableView(kSectionNoteCellTypes, self.tableView);
  }

  if (isCreating)
    return;
  m_sections.push_back(MWMEditorSectionButton);
  m_cells[MWMEditorSectionButton] = kSectionButtonCellTypes;
  registerCellsForTableView(kSectionButtonCellTypes, self.tableView);
}

- (MWMEditorCellID)cellTypeForIndexPath:(NSIndexPath *)indexPath
{
  return m_cells[m_sections[indexPath.section]][indexPath.row];
}

- (Class)cellClassForIndexPath:(NSIndexPath *)indexPath
{
  return cellClass([self cellTypeForIndexPath:indexPath]);
}

#pragma mark - Fill cells with data

- (void)configTextViewCell:(UITableViewCell * _Nonnull)cell
                    cellID:(MWMEditorCellID)cellID
                      icon:(NSString *)icon
               placeholder:(NSString * _Nonnull)name
              errorMessage:(NSString * _Nonnull)error
                   isValid:(BOOL)isValid
              keyboardType:(UIKeyboardType)keyboard
{
  MWMEditorTextTableViewCell * tCell = static_cast<MWMEditorTextTableViewCell *>(cell);
  [tCell configWithDelegate:self
                       icon:[UIImage imageNamed:icon]
                       text:ToNSString(m_mapObject.GetMetadata(static_cast<MetadataID>(cellID)))
                placeholder:name
               errorMessage:error
                    isValid:isValid
               keyboardType:keyboard
             capitalization:UITextAutocapitalizationTypeNone];
}

- (void)configTextViewCell:(UITableViewCell * _Nonnull)cell
                    cellID:(MWMEditorCellID)cellID
                      icon:(NSString * _Nonnull)icon
               placeholder:(NSString * _Nonnull)name
{
  MetadataID metaId = static_cast<MetadataID>(cellID);
  NSString* value = ToNSString(m_mapObject.GetMetadata(metaId));
  if (osm::isSocialContactTag(metaId) && [value containsString:@"/"])
    value = ToNSString(osm::socialContactToURL(metaId, [value UTF8String]));

  MWMEditorTextTableViewCell * tCell = static_cast<MWMEditorTextTableViewCell *>(cell);
  [tCell configWithDelegate:self
                       icon:[UIImage imageNamed:icon]
                       text:value
                placeholder:name
               keyboardType:UIKeyboardTypeDefault
             capitalization:UITextAutocapitalizationTypeSentences];
}

- (void)fillCell:(UITableViewCell * _Nonnull)cell atIndexPath:(NSIndexPath * _Nonnull)indexPath
{
  BOOL const isValid = ![self.invalidCells containsObject:indexPath];
  MWMEditorCellID const cellID = [self cellTypeForIndexPath:indexPath];
  switch (cellID)
  {
  case MWMEditorCellTypeCategory:
  {
    auto types = m_mapObject.GetTypes();
    types.SortBySpec();
    auto const readableType = classif().GetReadableObjectName(*(types.begin()));
    MWMEditorCategoryCell * cCell = static_cast<MWMEditorCategoryCell *>(cell);
    [cCell configureWithDelegate:self
                     detailTitle:@(platform::GetLocalizedTypeName(readableType).c_str())
                      isCreating:self.isCreating];
    break;
  }
  case MetadataID::FMD_PHONE_NUMBER:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_phone_number"
                 placeholder:L(@"phone")
                errorMessage:L(@"error_enter_correct_phone")
                     isValid:isValid
                keyboardType:UIKeyboardTypeNamePhonePad];
    break;
  }
  case MetadataID::FMD_WEBSITE:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_website"
                 placeholder:L(@"website")
                errorMessage:L(@"error_enter_correct_web")
                     isValid:isValid
                keyboardType:UIKeyboardTypeURL];
    break;
  }
  case MetadataID::FMD_WEBSITE_MENU:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_website_menu"
                 placeholder:L(@"website_menu")
                errorMessage:L(@"error_enter_correct_web")
                     isValid:isValid
                keyboardType:UIKeyboardTypeURL];
    break;
  }
  case MetadataID::FMD_EMAIL:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_email"
                 placeholder:L(@"email")
                errorMessage:L(@"error_enter_correct_email")
                     isValid:isValid
                keyboardType:UIKeyboardTypeEmailAddress];
    break;
  }
  case MetadataID::FMD_OPERATOR:
  {
    MWMEditorTextTableViewCell * tCell = static_cast<MWMEditorTextTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_operator"]
                         text:ToNSString(m_mapObject.GetMetadata(static_cast<MetadataID>(cellID)))
                  placeholder:L(@"editor_operator")
                 keyboardType:UIKeyboardTypeDefault
               capitalization:UITextAutocapitalizationTypeSentences];
    break;
  }
  case MetadataID::FMD_OPEN_HOURS:
  {
    MWMPlacePageOpeningHoursCell * tCell = static_cast<MWMPlacePageOpeningHoursCell *>(cell);
    NSString * text = ToNSString(m_mapObject.GetOpeningHours());
    [tCell configWithDelegate:self info:(text.length ? text : L(@"add_opening_hours"))];
    break;
  }
  case MetadataID::FMD_INTERNET:
  {
    MWMEditorSwitchTableViewCell * tCell = static_cast<MWMEditorSwitchTableViewCell *>(cell);
    // TODO(Vlad, IgorTomko): Support all other possible Internet statuses.
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_wifi"]
                         text:L(@"category_wifi")
                           on:m_mapObject.GetInternet() == feature::Internet::Wlan];
    break;
  }
  case MWMEditorCellTypeAdditionalName:
  {
    MWMEditorAdditionalNameTableViewCell * tCell =
        static_cast<MWMEditorAdditionalNameTableViewCell *>(cell);
    auto const & localizedNames = m_mapObject.GetNamesDataSource().names;
    if (indexPath.row < localizedNames.size())
    {
      osm::LocalizedName const & name = localizedNames[indexPath.row];
      NSString * langName = indexPath.row == StringUtf8Multilang::kDefaultCode ? L(@"editor_default_language_hint") : ToNSString(name.m_langName);
      [tCell configWithDelegate:self
                       langCode:name.m_code
                       langName:langName
                           name:@(name.m_name.c_str())
                   errorMessage:L(@"error_enter_correct_name")
                        isValid:isValid
                   keyboardType:UIKeyboardTypeDefault];
    }
    else
    {
      NSInteger const newAdditionalNameIndex = indexPath.row - localizedNames.size();
      NSInteger const langCode = m_newAdditionalLanguages[newAdditionalNameIndex];

      std::string name;
      // Default name can be changed in advanced mode.
      if (langCode == StringUtf8Multilang::kDefaultCode)
      {
        name = m_mapObject.GetDefaultName();
      }

      [tCell configWithDelegate:self
                       langCode:langCode
                       langName:ToNSString(StringUtf8Multilang::GetLangNameByCode(langCode))
                           name:@(name.c_str())
                   errorMessage:L(@"error_enter_correct_name")
                        isValid:isValid
                   keyboardType:UIKeyboardTypeDefault];
    }
    break;
  }
  case MWMEditorCellTypeAddAdditionalName:
  {
    [static_cast<MWMEditorAddAdditionalNameTableViewCell *>(cell) configWithDelegate:self];
    break;
  }
  case MWMEditorCellTypeAddAdditionalNamePlaceholder: break;
  case MWMEditorCellTypeStreet:
  {
    MWMEditorSelectTableViewCell * tCell = static_cast<MWMEditorSelectTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_address"]
                         text:@(m_mapObject.GetStreet().m_defaultName.c_str())
                  placeholder:L(@"add_street")];
    break;
  }
  case MWMEditorCellTypeBuilding:
  {
    MWMEditorTextTableViewCell * tCell = static_cast<MWMEditorTextTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:nil
                         text:@(m_mapObject.GetHouseNumber().c_str())
                  placeholder:L(@"house_number")
                 errorMessage:L(@"error_enter_correct_house_number")
                      isValid:isValid
                 keyboardType:UIKeyboardTypeDefault
               capitalization:UITextAutocapitalizationTypeNone];
    break;
  }
  case MetadataID::FMD_POSTCODE:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:nil
                 placeholder:L(@"editor_zip_code")
                errorMessage:L(@"error_enter_correct_zip_code")
                     isValid:isValid
                keyboardType:UIKeyboardTypeDefault];
    static_cast<MWMEditorTextTableViewCell *>(cell).textField.autocapitalizationType = UITextAutocapitalizationTypeAllCharacters;
    break;
  }
  case MetadataID::FMD_BUILDING_LEVELS:
  {
    NSString * placeholder =
        [NSString stringWithFormat:L(@"editor_storey_number"),
                                   osm::EditableMapObject::kMaximumLevelsEditableByUsers];
    NSString * errorMessage =
        [NSString stringWithFormat:L(@"error_enter_correct_storey_number"),
                                   osm::EditableMapObject::kMaximumLevelsEditableByUsers];
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:nil
                 placeholder:placeholder
                errorMessage:errorMessage
                     isValid:isValid
                keyboardType:UIKeyboardTypeNumberPad];
    break;
  }
  case MetadataID::FMD_LEVEL:
  {
    /// @todo Is it ok to use the same error string as in building levels?
    NSString * errorMessage =
        [NSString stringWithFormat:L(@"error_enter_correct_storey_number"),
                                   osm::EditableMapObject::kMaximumLevelsEditableByUsers];

    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_level"
                 placeholder:L(@"level")
                errorMessage:errorMessage
                     isValid:isValid
                keyboardType:UIKeyboardTypeNumbersAndPunctuation];
    break;
  }
  case MetadataID::FMD_CUISINE:
  {
    MWMEditorSelectTableViewCell * tCell = static_cast<MWMEditorSelectTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_cuisine"]
                         text:@(m_mapObject.FormatCuisines().c_str())
                  placeholder:L(@"select_cuisine")];
    break;
  }
  case MetadataID::FMD_DRIVE_THROUGH:
  {
    MWMEditorSegmentedTableViewCell * tCell = static_cast<MWMEditorSegmentedTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_drive_through"]
                         text:L(@"drive_through")
                         value:feature::YesNoUnknownFromString(m_mapObject.GetMetadata(feature::Metadata::FMD_DRIVE_THROUGH))];
    break;
  }
  case MetadataID::FMD_SELF_SERVICE:
  {
    MWMEditorSegmentedTableViewCell * tCell = static_cast<MWMEditorSegmentedTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_self_service"]
                         text:L(@"self_service")
                         value:feature::YesNoUnknownFromString(m_mapObject.GetMetadata(feature::Metadata::FMD_SELF_SERVICE))];
    break;
  }
  case MetadataID::FMD_OUTDOOR_SEATING:
  {
    MWMEditorSegmentedTableViewCell * tCell = static_cast<MWMEditorSegmentedTableViewCell *>(cell);
    [tCell configWithDelegate:self
                         icon:[UIImage imageNamed:@"ic_placepage_outdoor_seating"]
                         text:L(@"outdoor_seating")
                         value:feature::YesNoUnknownFromString(m_mapObject.GetMetadata(feature::Metadata::FMD_OUTDOOR_SEATING))];
    break;
  } 
  case MetadataID::FMD_CONTACT_FACEBOOK:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_facebook"
                 placeholder:L(@"facebook")];
    break;
  }
  case MetadataID::FMD_CONTACT_INSTAGRAM:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_instagram"
                 placeholder:L(@"instagram")];
    break;
  }
  case MetadataID::FMD_CONTACT_TWITTER:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_twitter"
                 placeholder:L(@"twitter")];
    break;
  }
  case MetadataID::FMD_CONTACT_VK:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_vk"
                 placeholder:L(@"vk")];
    break;
  }
  case MetadataID::FMD_CONTACT_LINE:
  {
    [self configTextViewCell:cell
                      cellID:cellID
                        icon:@"ic_placepage_line"
                 placeholder:L(@"line")];
    break;
  }
  case MWMEditorCellTypeNote:
  {
    MWMNoteCell * tCell = static_cast<MWMNoteCell *>(cell);
    [tCell configWithDelegate:self
                     noteText:self.note
                  placeholder:L(@"editor_detailed_description_hint")];
    break;
  }
  case MWMEditorCellTypeReportButton:
  {
    MWMButtonCell * tCell = static_cast<MWMButtonCell *>(cell);

    auto title = ^NSString *(FeatureStatus s, BOOL isUploaded)
    {
      if (isUploaded)
        return L(@"editor_place_doesnt_exist");
      switch (s)
      {
      case FeatureStatus::Untouched: return L(@"editor_place_doesnt_exist");
      case FeatureStatus::Deleted:
      case FeatureStatus::Obsolete:  // TODO(Vlad): Either make a valid button or disable it.
        NSAssert(false, @"Incorrect feature status!");
        return L(@"editor_place_doesnt_exist");
      case FeatureStatus::Modified: return L(@"editor_reset_edits_button");
      case FeatureStatus::Created: return L(@"editor_remove_place_button");
      }
    };

    [tCell configureWithDelegate:self title:title(self.featureStatus, self.isFeatureUploaded) enabled: YES];
    break;
  }
  default:
    NSAssert(false, @"Invalid field for editor: %d", (int)cellID);
    break;
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell * _Nonnull)tableView:(UITableView * _Nonnull)tableView
                  cellForRowAtIndexPath:(NSIndexPath * _Nonnull)indexPath
{
  Class cls = [self cellClassForIndexPath:indexPath];
  auto cell = [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath];
  [self fillCell:cell atIndexPath:indexPath];
  return cell;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView * _Nonnull)tableView
{
  return m_sections.size();
}

- (NSInteger)tableView:(UITableView * _Nonnull)tableView numberOfRowsInSection:(NSInteger)section
{
  return m_cells[m_sections[section]].size();
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView * _Nonnull)tableView
    heightForRowAtIndexPath:(NSIndexPath * _Nonnull)indexPath
{
  Class cls = [self cellClassForIndexPath:indexPath];
  auto cell = [self offscreenCellForClass:cls];
  [self fillCell:cell atIndexPath:indexPath];
  switch ([self cellTypeForIndexPath:indexPath])
  {
  case MetadataID::FMD_OPEN_HOURS: return ((MWMPlacePageOpeningHoursCell *)cell).cellHeight;
  case MWMEditorCellTypeCategory:
  case MWMEditorCellTypeReportButton: return self.tableView.rowHeight;
  case MWMEditorCellTypeNote: return UITableViewAutomaticDimension;
  default:
  {
    [cell setNeedsUpdateConstraints];
    [cell updateConstraintsIfNeeded];
    cell.bounds = {{}, {CGRectGetWidth(tableView.bounds), CGRectGetHeight(cell.bounds)}};
    [cell setNeedsLayout];
    [cell layoutIfNeeded];
    CGSize const size = [cell.contentView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
    return size.height;
  }
  }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
  switch (m_sections[section])
  {
  case MWMEditorSectionAdditionalNames:
  case MWMEditorSectionCategory:
  case MWMEditorSectionButton: return nil;
  case MWMEditorSectionNote: return L(@"editor_other_info");
  case MWMEditorSectionAddress: return L(@"address");
  case MWMEditorSectionDetails: return L(@"details");
  }
}

- (UIView *)tableView:(UITableView *)tableView viewForHeaderInSection:(NSInteger)section
{
  switch (m_sections[section])
  {
  case MWMEditorSectionAdditionalNames: return self.additionalNamesHeader;
  case MWMEditorSectionCategory:
  case MWMEditorSectionButton:
  case MWMEditorSectionNote:
  case MWMEditorSectionAddress:
  case MWMEditorSectionDetails: return nil;
  }
}

- (UIView *)tableView:(UITableView *)tableView viewForFooterInSection:(NSInteger)section
{
  switch (m_sections[section])
  {
  case MWMEditorSectionAddress:
  case MWMEditorSectionCategory:
  case MWMEditorSectionAdditionalNames:
  case MWMEditorSectionButton: return nil;
  case MWMEditorSectionDetails:
    if (find(m_sections.begin(), m_sections.end(), MWMEditorSectionNote) == m_sections.end())
      return self.notesFooter;
    return nil;
  case MWMEditorSectionNote: return self.notesFooter;
  }
}

- (CGFloat)tableView:(UITableView *)tableView heightForHeaderInSection:(NSInteger)section
{
  return m_sections[section] == MWMEditorSectionNote ? kDefaultHeaderHeight * 2 : kDefaultHeaderHeight;
}

- (CGFloat)tableView:(UITableView *)tableView heightForFooterInSection:(NSInteger)section
{
  switch (m_sections[section])
  {
  case MWMEditorSectionAddress:
      return 1.0;
  case MWMEditorSectionDetails:
    if (find(m_sections.begin(), m_sections.end(), MWMEditorSectionNote) == m_sections.end())
      return self.notesFooter.height;
    return 1.0;
  case MWMEditorSectionNote: return self.notesFooter.height;
  case MWMEditorSectionCategory:
  case MWMEditorSectionAdditionalNames:
  case MWMEditorSectionButton: return kDefaultFooterHeight;
  }
}

#pragma mark - MWMPlacePageOpeningHoursCellProtocol

- (BOOL)forcedButton { return YES; }
- (BOOL)isPlaceholder { return m_mapObject.GetOpeningHours().empty(); }
- (BOOL)isEditor { return YES; }
- (BOOL)openingHoursCellExpanded { return YES; }
- (void)setOpeningHoursCellExpanded:(BOOL)openingHoursCellExpanded
{
  [self performSegueWithIdentifier:kOpeningHoursEditorSegue sender:nil];
}

- (void)markCellAsInvalid:(NSIndexPath *)indexPath
{
  if (![self.invalidCells containsObject:indexPath])
    [self.invalidCells addObject:indexPath];

  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationFade];
}

#pragma mark - MWMNoteCellDelegate

- (void)cell:(MWMNoteCell *)cell didChangeSizeAndText:(NSString *)text
{
  self.offscreenCells[(id<NSCopying>)cellClass(MWMEditorCellTypeNote)] = cell;
  self.note = text;
  [UIView setAnimationsEnabled:NO];
  [self.tableView refresh];
  [UIView setAnimationsEnabled:YES];
  NSIndexPath * ip = [self.tableView indexPathForCell:cell];
  [self.tableView scrollToRowAtIndexPath:ip
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:YES];
}

- (void)cell:(MWMNoteCell *)cell didFinishEditingWithText:(NSString *)text { self.note = text; }
#pragma mark - MWMEditorAdditionalName

- (void)editAdditionalNameLanguage:(NSInteger)selectedLangCode
{
  [self performSegueWithIdentifier:kAdditionalNamesEditorSegue sender:@(selectedLangCode)];
}

#pragma mark - MWMEditorAdditionalNamesProtocol

- (void)addAdditionalName:(NSInteger)languageIndex
{
  m_newAdditionalLanguages.push_back(languageIndex);
  self.showAdditionalNames = YES;
  auto additionalNamesSectionIt =
      find(m_sections.begin(), m_sections.end(), MWMEditorSectionAdditionalNames);
  assert(additionalNamesSectionIt != m_sections.end());
  auto const section = distance(m_sections.begin(), additionalNamesSectionIt);
  NSInteger const row = [self tableView:self.tableView numberOfRowsInSection:section];
  assert(row > 0);
  NSIndexPath * indexPath = [NSIndexPath indexPathForRow:row - 1 inSection:section];
  [self.tableView scrollToRowAtIndexPath:indexPath
                        atScrollPosition:UITableViewScrollPositionMiddle
                                animated:NO];
}

#pragma mark - MWMEditorCellProtocol

- (void)tryToChangeInvalidStateForCell:(MWMTableViewCell *)cell
{
  [self.tableView update:^{
    NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
    [self.invalidCells removeObject:indexPath];
  }];
}

- (void)cell:(MWMTableViewCell *)cell changedText:(NSString *)changeText
{
  NSAssert(changeText != nil, @"String can't be nil!");
  NSIndexPath * indexPath = [self.tableView indexPathForRowAtPoint:cell.center];
  MWMEditorCellID const cellType = [self cellTypeForIndexPath:indexPath];
  std::string val = changeText.UTF8String;
  BOOL isFieldValid = YES;
  switch (cellType)
  {
  case MWMEditorCellTypeBuilding:
    m_mapObject.SetHouseNumber(val);
    isFieldValid = osm::EditableMapObject::ValidateHouseNumber(val);
    break;
  case MWMEditorCellTypeAdditionalName:
    isFieldValid = osm::EditableMapObject::ValidateName(val);
    if (isFieldValid)
      m_mapObject.SetName(val, static_cast<MWMEditorAdditionalNameTableViewCell *>(cell).code);
    break;
  default:
    auto const metadataID = static_cast<MetadataID>(cellType);
    ASSERT_LESS(metadataID, MetadataID::FMD_COUNT, ());
    isFieldValid = osm::EditableMapObject::IsValidMetadata(metadataID, val)? YES : NO;
    m_mapObject.SetMetadata(metadataID, std::move(val));
    break;
  }

  if (!isFieldValid)
    [self markCellAsInvalid:indexPath];
}

- (void)cell:(UITableViewCell *)cell changeSwitch:(BOOL)changeSwitch
{
  NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
  switch ([self cellTypeForIndexPath:indexPath])
  {
  case MetadataID::FMD_INTERNET:
    m_mapObject.SetInternet(changeSwitch ? feature::Internet::Wlan : feature::Internet::Unknown);
    break;
  default: NSAssert(false, @"Invalid field for changeSwitch"); break;
  }
}

- (void)cell:(UITableViewCell *)cell changeSegmented:(YesNoUnknown)changeSegmented
{
  NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
  switch ([self cellTypeForIndexPath:indexPath])
  {
  case MetadataID::FMD_DRIVE_THROUGH:
      switch (changeSegmented)
      {
        case Yes:
          m_mapObject.SetMetadata(feature::Metadata::FMD_DRIVE_THROUGH, "yes");
          break;
        case No:
          m_mapObject.SetMetadata(feature::Metadata::FMD_DRIVE_THROUGH, "no");
          break;
        case Unknown:
          m_mapObject.SetMetadata(feature::Metadata::FMD_DRIVE_THROUGH, "");
          break;
      }
      break;

    case MetadataID::FMD_SELF_SERVICE:
      switch (changeSegmented)
      {
        case Yes:
          m_mapObject.SetMetadata(feature::Metadata::FMD_SELF_SERVICE, "yes");
          break;
        case No:
          m_mapObject.SetMetadata(feature::Metadata::FMD_SELF_SERVICE, "no");
          break;
        case Unknown:
          m_mapObject.SetMetadata(feature::Metadata::FMD_SELF_SERVICE, "");
          break;
      }
      break;

    case MetadataID::FMD_OUTDOOR_SEATING:
      switch (changeSegmented)
      {
        case Yes:
          m_mapObject.SetMetadata(feature::Metadata::FMD_OUTDOOR_SEATING, "yes");
          break;
        case No:
          m_mapObject.SetMetadata(feature::Metadata::FMD_OUTDOOR_SEATING, "no");
          break;
        case Unknown:
          m_mapObject.SetMetadata(feature::Metadata::FMD_OUTDOOR_SEATING, "");
          break;
      }
      break;

  default: NSAssert(false, @"Invalid field for changeSegmented"); break;
  }
}

#pragma mark - MWMEditorCellProtocol && MWMButtonCellDelegate

- (void)cellDidPressButton:(UITableViewCell *)cell
{
  NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
  switch ([self cellTypeForIndexPath:indexPath])
  {
  case MWMEditorCellTypeStreet:
    [self performSegueWithIdentifier:kStreetEditorSegue sender:nil];
    break;
  case MetadataID::FMD_CUISINE:
    [self performSegueWithIdentifier:kCuisineEditorSegue sender:nil];
    break;
  case MWMEditorCellTypeCategory:
    [self performSegueWithIdentifier:kCategoryEditorSegue sender:nil];
    break;
  case MWMEditorCellTypeReportButton: [self tapOnButtonCell:cell]; break;
  default: NSAssert(false, @"Invalid field for cellSelect"); break;
  }
}

- (void)tapOnButtonCell:(UITableViewCell *)cell
{
  auto const & fid = m_mapObject.GetID();
  self.isFeatureUploaded = osm::Editor::Instance().IsFeatureUploaded(fid.m_mwmId, fid.m_index);
  NSIndexPath * ip = [self.tableView indexPathForCell:cell];
  [self.tableView reloadRowsAtIndexPaths:@[ ip ] withRowAnimation:UITableViewRowAnimationFade];

  auto placeDoesntExistAction = ^{
    [self.alertController presentPlaceDoesntExistAlertWithBlock:^(NSString * additionalMessage) {
      std::string const additional = additionalMessage.length ? additionalMessage.UTF8String : "";
      GetFramework().CreateNote(self->m_mapObject, osm::Editor::NoteProblemType::PlaceDoesNotExist,
                                additional);
      [self goBack];
      [self showNotesQueuedToast];
    }];
  };

  auto revertAction = ^(BOOL isCreated) {
    auto & f = GetFramework();
    if (!f.RollBackChanges(fid))
      NSAssert(false, @"We shouldn't call this if we can't roll back!");

    f.GetSearchAPI().PokeSearchInViewport();
    [self goBack];
  };

  if (self.isFeatureUploaded)
  {
    placeDoesntExistAction();
  }
  else
  {
    switch (self.featureStatus)
    {
    case FeatureStatus::Untouched: placeDoesntExistAction(); break;
    case FeatureStatus::Modified:
    {
      [self.alertController presentResetChangesAlertWithBlock:^{
        revertAction(NO);
      }];
      break;
    }
    case FeatureStatus::Created:
    {
      [self.alertController presentDeleteFeatureAlertWithBlock:^{
        revertAction(YES);
      }];
      break;
    }
    case FeatureStatus::Deleted: break;
    case FeatureStatus::Obsolete: break;
    }
  }
}

#pragma mark - MWMOpeningHoursEditorProtocol

- (void)setOpeningHours:(NSString *)openingHours
{
  m_mapObject.SetOpeningHours(openingHours.UTF8String);
}

#pragma mark - MWMObjectsCategorySelectorDelegate

- (void)reloadObject:(osm::EditableMapObject const &)object
{
  [self setEditableMapObject:object];
  [self configTable];
}

#pragma mark - MWMCuisineEditorProtocol

- (std::vector<std::string>)selectedCuisines { return m_mapObject.GetCuisines(); }
- (void)setSelectedCuisines:(std::vector<std::string> const &)cuisines { m_mapObject.SetCuisines(cuisines); }
#pragma mark - MWMStreetEditorProtocol

- (void)setNearbyStreet:(osm::LocalizedStreet const &)street { m_mapObject.SetStreet(street); }
- (osm::LocalizedStreet const &)currentStreet { return m_mapObject.GetStreet(); }
- (std::vector<osm::LocalizedStreet> const &)nearbyStreets { return m_mapObject.GetNearbyStreets(); }
#pragma mark - Segue

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:kOpeningHoursEditorSegue])
  {
    MWMOpeningHoursEditorViewController * dvc = segue.destinationViewController;
    dvc.openingHours = ToNSString(m_mapObject.GetOpeningHours());
    dvc.delegate = self;
  }
  else if ([segue.identifier isEqualToString:kCuisineEditorSegue])
  {
    MWMCuisineEditorViewController * dvc = segue.destinationViewController;
    dvc.delegate = self;
  }
  else if ([segue.identifier isEqualToString:kStreetEditorSegue])
  {
    MWMStreetEditorViewController * dvc = segue.destinationViewController;
    dvc.delegate = self;
  }
  else if ([segue.identifier isEqualToString:kCategoryEditorSegue])
  {
    NSAssert(self.isCreating, @"Invalid state! We'll be able to change feature category only if we "
                              @"are creating feature!");
    MWMObjectsCategorySelectorController * dvc = segue.destinationViewController;
    dvc.delegate = self;
    auto const type = *(m_mapObject.GetTypes().begin());
    auto const readableType = classif().GetReadableObjectName(type);
    [dvc setSelectedCategory:readableType];
  }
  else if ([segue.identifier isEqualToString:kAdditionalNamesEditorSegue])
  {
    MWMEditorAdditionalNamesTableViewController * dvc = segue.destinationViewController;
    [dvc configWithDelegate:self name:m_mapObject.GetNameMultilang() additionalSkipLanguageCodes:m_newAdditionalLanguages];
  }
}

#pragma mark - Alert

- (BOOL)showPersonalInfoWarningAlertIfNeeded
{
  NSUserDefaults * ud = NSUserDefaults.standardUserDefaults;
  if ([ud boolForKey:kUDEditorPersonalInfoWarninWasShown])
    return NO;

  [self.alertController presentPersonalInfoWarningAlertWithBlock:^
  {
    [ud setBool:YES forKey:kUDEditorPersonalInfoWarninWasShown];
    [self onSave];
  }];

  return YES;
}

@end
