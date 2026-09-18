#include <stdio.h>
#include <iostream>


__attribute__((noinline))
int add(int a, int b){
  return a + b;
}

int cal(int a) {
  int n = 0;
  for (int i = 0; i < 100; ++i) {
    n = add(n, a);
  }
  return n;
}
int main(int argc, char *argv[]) {
  int result = cal(3);
  int c = result;
  return 0;
}

/*
**如果你能亲手指出 `call Add` 消失了，并且解释“为什么消失”，我们直接给你发一个真正有含金量的成就：**

> 🏆 **Compiler Optimization Level 1：Inline Detective**
> 

然后我们马上从 Inline 转到一个你应该会明显更兴奋的东西：

> **“为什么一个 `virtual` 函数可能阻碍这种优化？”**
>
   [Observation]
   - Code
int cal(int a) {
  int n = 0;
  for (int i = 0; i < 100; ++i) {
    n = add(n, a);
  }
  return n;
}
   - o3noinline                             - o3inline
   	.cfi_def_cfa_register %rbp               	.cfi_def_cfa_register %rbp    
	pushq	%r14                                 	imull	$100, %edi, %eax      
	pushq	%rbx                                 	popq	%rbp                   
	.cfi_offset %rbx, -32                      	retq                         
	.cfi_offset %r14, -24                          
	movl	%edi, %r14d
	xorl	%eax, %eax
	movl	$100, %ebx
	.p2align	4, 0x90
LBB1_1:  ## =>This Inner Loop Header: Depth=1
	movl	%eax, %edi
	movl	%r14d, %esi
	callq	__Z3addii
	decl	%ebx
	jne	LBB1_1
## %bb.2:
	popq	%rbx
	popq	%r14
	popq	%rbp
	retq

  - 汇编代码中很容易看出来 `add` 没有了
   [Hypothesise]
   - 至于为什么，我知道是因为被内联了，但是循环是怎么实现的呢？
   - 或许 `imull` 就是编译器用来迭代的方法，因为看到了 `100` 
   [Uncertain]
   [Experiment]
   [Result]
   [Conclusion]

   */
