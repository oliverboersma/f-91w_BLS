"""Minimal S-expression parser for KiCad files: lists, symbols (str) and quoted strings (Q)."""
class Q(str):
    pass
def parse(text):
    stack=[[]]; i=0; n=len(text)
    while i<n:
        c=text[i]
        if c=='(':
            stack.append([]); i+=1
        elif c==')':
            x=stack.pop(); stack[-1].append(x); i+=1
        elif c=='"':
            j=i+1; buf=[]
            while text[j]!='"':
                if text[j]=='\\': buf.append(text[j+1]); j+=2
                else: buf.append(text[j]); j+=1
            stack[-1].append(Q(''.join(buf))); i=j+1
        elif c.isspace(): i+=1
        else:
            j=i
            while j<n and not text[j].isspace() and text[j] not in '()': j+=1
            stack[-1].append(text[i:j]); i=j
    return stack[0][0]
def find(node,key): return [x for x in node if isinstance(x,list) and x and x[0]==key]
def one(node,key,default=None):
    r=find(node,key); return r[0] if r else default
