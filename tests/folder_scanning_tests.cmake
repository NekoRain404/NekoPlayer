add_executable(folder_scan_test folder_scan_test.cpp)
target_link_libraries(folder_scan_test PRIVATE neko_input Qt6::Test)
add_test(NAME folder-scan COMMAND folder_scan_test)
set_tests_properties(folder-scan PROPERTIES TIMEOUT 60)

add_executable(folder_menu_test folder_menu_test.cpp)
target_link_libraries(folder_menu_test PRIVATE neko_ui Qt6::Test)
add_test(NAME folder-menu COMMAND folder_menu_test)
