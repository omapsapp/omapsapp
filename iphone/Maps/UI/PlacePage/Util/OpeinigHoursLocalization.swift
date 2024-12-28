import Foundation

@objcMembers
class OpeinigHoursLocalization: NSObject, IOpeningHoursLocalization {
  var closedString: String {
    L("closed")
  }

  var breakString: String {
    L("editor_hours_closed")
  }

  var twentyFourSevenString: String {
    L("twentyfour_seven")
  }

  var allDayString: String {
    L("editor_time_allday")
  }

  var dailyString: String {
    L("daily")
  }

  var todayString: String {
    L("today")
  }

  var dayOffString: String {
    L("day_off_today")
  }
}
