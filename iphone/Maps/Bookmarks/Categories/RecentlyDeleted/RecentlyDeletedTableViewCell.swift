final class RecentlyDeletedTableViewCell: UITableViewCell {

  struct ViewModel: Equatable, Hashable {
    let fileName: String
    let fileURL: URL
    let deletionDate: Date
  }

  override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
    super.init(style: .subtitle, reuseIdentifier: reuseIdentifier)
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  func configureWith(_ viewModel: ViewModel) {
    textLabel?.text = viewModel.fileName
    detailTextLabel?.text = DateTimeFormatter.dateString(from: viewModel.deletionDate,
                                                         dateStyle: .medium,
                                                         timeStyle: .medium)
  }
}

extension RecentlyDeletedTableViewCell.ViewModel {
  init(_ category: RecentlyDeletedCategory) {
    self.fileName = category.title
    self.fileURL = category.fileURL
    self.deletionDate = category.deletionDate
  }
}
