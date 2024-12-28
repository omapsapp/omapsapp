import Foundation

@objcMembers
public final class Product: NSObject {
  public let title: String
  public let link: String

  public init(title: String, link: String) {
    self.title = title
    self.link = link
  }
}

@objcMembers
public final class ProductsConfiguration: NSObject {
  public let placePagePrompt: String
  public let products: [Product]

  public init(placePagePrompt: String, products: [Product]) {
    self.placePagePrompt = placePagePrompt
    self.products = products
  }
}
