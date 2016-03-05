from collections import deque

def find_cycle(graph, root):
    color = {}
    stack = [root]

    for u in graph.keys():
        color[u] = 'white'

    while len(stack) != 0:
        print stack
        u = stack[-1]
        if color[u] == 'white':
            color[u] = 'gray'
            for v in graph[u]:
                if color[v] == 'white':
                    stack.append(v)
                elif color[v] == 'gray':
                    print 'found back edge: ' + str((u,v))

        elif color[u] == 'gray':
            color[u] = 'black'
            stack.pop()

def unfold_cycle(graph, (a,b), k, kmax):
    if k < kmax:
        queue  = deque([b])
        closed = []
        unfold = False
        while len(queue) != 0:
            u = queue.popleft()
            closed.append(u)
            graph[u + '*'] = []    
            for v in graph[u]:
                graph[u + '*'].append(v + '*')
                if (u,v) != (a,b):
                    if v not in closed:
                        queue.append(v)
                else:
                    unfold = True
        
        if unfold:
            unfold_cycle(graph, (a + '*', b + '*'), k+1, kmax)

        graph[a].append(b + '*')
    
    graph[a].remove(b)

def main():
    test = {
        'a': ['b', 'c'],
        'b': ['d'],
        'c': ['d', 'f'],
        'd': ['e', 'f'],
        'e': ['e', 'c'],
        'f': ['g'],
        'g': ['a']
        }
    
    #find_cycle(test, 'a')
    unfold_cycle(test, ('e', 'c'), 0, 2)

    for node in test:
        print node + '\t:\t' + str(test[node])

if __name__ == "__main__":
    main()