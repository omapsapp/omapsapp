final class BookmarksListRouter {
  private let mapViewController: MapViewController
  private weak var coordinator: BookmarksCoordinator?

  init(_ mapViewController: MapViewController, bookmarksCoordinator: BookmarksCoordinator?) {
    self.mapViewController = mapViewController
    self.coordinator = bookmarksCoordinator
  }
}

extension BookmarksListRouter: IBookmarksListRouter {
  func listSettings(_ bookmarkGroup: BookmarkGroup, delegate: CategorySettingsViewControllerDelegate?) {
    let listSettingsController = CategorySettingsViewController(bookmarkGroup: bookmarkGroup)
    listSettingsController.delegate = delegate
    mapViewController.navigationController?.pushViewController(listSettingsController, animated: true)
  }
  
  func sharingOptions(_ bookmarkGroup: BookmarkGroup) {
    let storyboard = UIStoryboard.instance(.sharing)
    let shareController = storyboard.instantiateInitialViewController() as! BookmarksSharingViewController
    shareController.category = bookmarkGroup
    mapViewController.navigationController?.pushViewController(shareController, animated: true)
  }

  func viewOnMap(_ bookmarkGroup: BookmarkGroup) {
    coordinator?.hide(categoryId: bookmarkGroup.categoryId)
  }

  func showDescription(_ bookmarkGroup: BookmarkGroup) {
//    let descriptionViewController = GuideDescriptionViewController(category: bookmarkGroup)
//    mapViewController.navigationController?.pushViewController(descriptionViewController, animated: true)
  }

  func showSubgroup(_ subgroupId: MWMMarkGroupID) {
    let bookmarksListViewController = BookmarksListBuilder.build(markGroupId: subgroupId,
                                                                 bookmarksCoordinator: coordinator)
    mapViewController.navigationController?.pushViewController(bookmarksListViewController, animated: true)
  }
}
