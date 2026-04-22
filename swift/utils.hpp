//
//  p12_password_check.hpp
//  feather
//
//  Created by HAHALOSAH on 8/6/24.
//

#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif
bool p12_password_check(NSString *file, NSString *pass);
void password_check_fix(NSString *path);
void password_check_fix_free(NSString *path);
#ifdef __cplusplus
}
#endif
