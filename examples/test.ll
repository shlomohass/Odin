declare i32 @putchar(i32)

define void @main() {
entry:
	%0 = alloca <8 x i1>, align 8 ; v
	store <8 x i1> zeroinitializer, <8 x i1>* %0
	%1 = alloca <8 x i1>, align 8
	store <8 x i1> zeroinitializer, <8 x i1>* %1
	%2 = load <8 x i1>, <8 x i1>* %1, align 8
	%3 = insertelement <8 x i1> %2, i1 true, i64 0
	%4 = insertelement <8 x i1> %3, i1 false, i64 1
	%5 = insertelement <8 x i1> %4, i1 true, i64 2
	%6 = insertelement <8 x i1> %5, i1 false, i64 3
	%7 = insertelement <8 x i1> %6, i1 true, i64 4
	%8 = insertelement <8 x i1> %7, i1 false, i64 5
	%9 = insertelement <8 x i1> %8, i1 true, i64 6
	%10 = insertelement <8 x i1> %9, i1 false, i64 7
	store <8 x i1> %10, <8 x i1>* %0

	%11 = load <8 x i1>, <8 x i1>* %0, align 8
	%12 = extractelement <8 x i1> %11, i64 0
	%13 = zext i1 %12 to i32
	%14 = add i32 %13, 65 ; + 'A'
	%15 = call i32 @putchar(i32 %14)

	%16 = load <8 x i1>, <8 x i1>* %0, align 8
	%17 = extractelement <8 x i1> %16, i64 1
	%18 = zext i1 %17 to i32
	%19 = add i32 %18, 65 ; + 'A'
	%20 = call i32 @putchar(i32 %19)

	%21 = load <8 x i1>, <8 x i1>* %0, align 8
	%22 = extractelement <8 x i1> %21, i64 2
	%23 = zext i1 %22 to i32
	%24 = add i32 %23, 65 ; + 'A'
	%25 = call i32 @putchar(i32 %24)

	%26 = call i32 @putchar(i32 10) ; \n

	ret void
}