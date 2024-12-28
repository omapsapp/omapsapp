struct ProductsViewModel {
  private let productsManager: ProductsManager.Type

  let title: String = L("support_organic_maps")
  let description: String
  let leadingSubtitle: String = L("already_donated")
  let trailingSubtitle: String = L("remind_me_later")
  let products: [Product]

  init(manager: ProductsManager.Type, configuration: ProductsConfiguration) {
    self.productsManager = manager
    self.description = configuration.placePagePrompt
    self.products = configuration.products
  }

  func didSelectProduct(_ product: Product) {
    UIViewController.topViewController().openUrl(product.link, externally: true)
    productsManager.didSelect(product)
  }

  func didClose(reason: ProductsPopupCloseReason) {
    productsManager.didCloseProductsPopup(with: reason)
  }
}
