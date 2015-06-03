/*
 Copyright Disney Enterprises, Inc.  All rights reserved.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License
 and the following modification to it: Section 6 Trademarks.
 deleted and replaced with:
 
 6. Trademarks. This License does not grant permission to use the
 trade names, trademarks, service marks, or product names of the
 Licensor and its affiliates, except as required for reproducing
 the content of the NOTICE file.
 
 You may obtain a copy of the License at
 http://www.apache.org/licenses/LICENSE-2.0
*/
#include "ExprFunc.h"
#include "ExprFuncX.h"
#include "Interpreter.h"
#include "ExprNode.h"
#include <cstdio>

namespace SeExpr2 {
int ExprFuncSimple::EvalOp(int* opData,double* fp,char** c,std::vector<int>& callStack)
{
    ExprFuncSimple* simple=reinterpret_cast<ExprFuncSimple*>(c[opData[0]]);
//    ExprFuncNode::Data* simpleData=reinterpret_cast<ExprFuncNode::Data*>(c[opData[1]]);
    ArgHandle args(opData,fp,c,callStack);
    simple->eval(args);
    return 1;
}

int ExprFuncSimple::buildInterpreter(const ExprFuncNode* node,Interpreter* interpreter) const
{
    std::vector<int> operands;
    for(int c=0;c<node->numChildren();c++){
        int operand=node->child(c)->buildInterpreter(interpreter);
        std::cerr<<"we are "<<node->promote(c)<<" "<<c<<std::endl;
        if(node->promote(c) != 0) {
            interpreter->addOp(getTemplatizedOp<Promote>(node->promote(c)));
            int promotedOperand=interpreter->allocFP(node->promote(c));
            interpreter->addOperand(operand);
            interpreter->addOperand(promotedOperand);
            operand=promotedOperand;
            interpreter->endOp();
        }
        operands.push_back(operand);
    }
    int outoperand=-1;
    int nargsData=interpreter->allocFP(1);
    interpreter->d[nargsData]=node->numChildren();
    if(node->type().isFP()) outoperand=interpreter->allocFP(node->type().dim());
    else if(node->type().isString()) outoperand=interpreter->allocPtr();
    else assert(false);

    interpreter->addOp(EvalOp);
    int ptrLoc=interpreter->allocPtr();
    int ptrDataLoc=interpreter->allocPtr();
    interpreter->s[ptrLoc]=(char*)this;
    interpreter->addOperand(ptrLoc); 
    interpreter->addOperand(ptrDataLoc);
    interpreter->addOperand(outoperand);
    interpreter->addOperand(nargsData); 
    for(size_t c=0;c<operands.size();c++){
        interpreter->addOperand(operands[c]);
    }
    interpreter->endOp(false); // do not eval because the function may not be evaluatable!

    // call into interpreter eval
    int pc=interpreter->nextPC()-1;
    int* opCurr=(&interpreter->opData[0])+interpreter->ops[pc].second;

    ArgHandle args(opCurr,&interpreter->d[0],&interpreter->s[0],interpreter->callStack);
    interpreter->s[ptrDataLoc]=reinterpret_cast<char*>(evalConstant(args));
    

    return outoperand;
}

}

extern "C" {
//            allocate int[4+number of args];
//            allocate char*[2];
//            allocate double[1+ sizeof(ret) + sizeof(args)]
//
//            int[0]= c , 0
//            int[1]= c , 1
//            int[2]= f,  0
//            int[3]= f,  8
//
//            int[4]= f, 8
//            int[5]= f, 9
//
//
//                    double[0] = 0
//                    double[1] = 0
//                    double[2] = 0
//                    double[3] = 0
// opData indexes either into f or into c.
// opdata[0] points to ExprFuncSimple instance
// opdata[1] points to the data generated by evalConstant
// opdata[2] points to return value
// opdata[3] points to number of args
// opdata[4] points to beginning of arguments in
void resolveCustomFunction(const char *name, int *opDataArg, int nargs,
                           double *fpArg, int fpArglen,
                           char **strArg, int strArglen,
                           void **funcdata, double *result, int retSize) {
    const SeExpr2::ExprFunc *func = SeExpr2::ExprFunc::lookup(name);
    SeExpr2::ExprFuncX *funcX = const_cast<SeExpr2::ExprFuncX*>(func->funcx());
    SeExpr2::ExprFuncSimple *funcSimple = static_cast<SeExpr2::ExprFuncSimple*>(funcX);

    char *c[2+strArglen];
    c[0] = reinterpret_cast<char*>(funcSimple);
    c[1] = 0;
    for(int i = 0; i < strArglen; ++i)
        c[2+i] = strArg[i];

    double fp[1+retSize+fpArglen];
    fp[0] = (double)nargs;
    for(int i = 0; i < retSize; ++i)
        fp[1+i] = 0;
    for(int i = 0; i < fpArglen; ++i)
        fp[1+retSize+i] = fpArg[i];

    int opData[4+nargs];
    opData[0] = 0;
    opData[1] = 1;
    opData[2] = 1;
    opData[3] = 0;
    for(int i = 0; i < nargs; ++i)
        opData[4+i] = opDataArg[i];

    std::vector<int> callStack;
    SeExpr2::ExprFuncSimple::ArgHandle handle(opData,fp,c,callStack);
    if(!*funcdata) {
        handle.data = funcSimple->evalConstant(handle);
        *funcdata = reinterpret_cast<void*>(handle.data);
    } else {
        handle.data = reinterpret_cast<SeExpr2::ExprFuncNode::Data*>(*funcdata);
    }

    funcSimple->eval(handle);
    for(int i = 0; i < retSize; ++i)
        result[i] = fp[1+i];
}

}


