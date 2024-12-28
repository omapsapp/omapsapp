@interface MWMObjectsCategorySelectorDataSource : NSObject

- (void)search:(NSString *)query;
- (NSString *)getTranslation:(NSInteger)row;
- (NSString *)getType:(NSInteger)row;
- (NSInteger)size;

@end
