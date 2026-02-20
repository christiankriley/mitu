mitu is an offline C++ phone number info lookup tool. It can provide the location, time zone, and current local time by inputting the full phone number. It implements the google/libphonenumber dataset as well as custom additions that may be configured by the user (not all prefixes, especially those for low population areas, are actually included). It follows the C++ Core Guidelines and aims for high optimization and safety.

TODO:
- Handle "unknown" city more elegantly, simply say the state or country for example
- Implement user config file to configure time format, dataset dir, sets to include (e.g. 1.txt for North America)
- Provide an update feature that will pull latest data from google/libphonenumber github repo and rebuild database
- Eliminate a single phone number matching an undefined amount of timezones (./nanper 15555555555 currently displays this behavior)
- Potentially add carrier information from google/libphonenumber
- Use std::jthread to parse data files simultaneously
- JSON output mode
- Store timestamp for db build and display a warning after x amount of time
- Check for file corruption
- Implement a test to ensure valid phone numbers match expected output

IMPLEMENTED:
- Support phone number formatting - spaces, (), +, -
- Implement some level of validation for phone numbers that cannot be real (nothing too robust but if outside of standard format, avoid wasting time). (We now check for non-digit characters & leading zeros to clean, then validate that the sanitized number does not surpass 15 digits)