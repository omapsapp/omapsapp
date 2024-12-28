import SwiftUI

struct AppLogo: View {
  var body: some View {
    Image(.logo)
      .resizable()
      .aspectRatio(contentMode: .fit)
  }
}
