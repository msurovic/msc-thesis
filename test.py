from collections import deque

def find_cycle(graph, root):
    color = {}

    for u in graph.keys():
        color[u] = 'white'

    stack = [root]
    color[root] = 'gray'

    while len(stack) != 0:
        u = stack[-1]
        done = True
        for v in graph[u]:
            if color[v] == 'white':
                done = False
                color[v] = 'gray'
                stack.append(v)
            elif color[v] == 'gray':
                return (u,v)

        if done:
            color[u] = 'black'
            stack.pop()

    return None

def get_id(graph, node):
    prefix = node[0]
    suffix = node[1:]
    
    while prefix + suffix in graph.keys():
        if suffix == '':
            suffix = '1'
        else:
            suffix = str(int(suffix) + 1)

    return prefix + suffix

def unfold_cycle(input, edge, max):
    for i in range(0, max):
        queue  = deque([edge[1]])
        closed = [edge[1]]
        output = {}
        bridge = None
        nwedge = None

        while len(queue) != 0:
            u = queue.popleft()
            a = get_id(input, u)
            output[a] = []
            for v in input[u]:
                b = get_id(input, v)
                output[a].append(b)

                if (u,v) == edge:
                    bridge = (u,b)
                    nwedge = (a,b)

                if v not in closed:
                    closed.append(v)
                    queue.append(v)

        input.update(output)

        if bridge is None or nwedge is None:
            print "BIAATCH"
            break

        if edge[1] in input[edge[0]]:
            input[edge[0]].remove(edge[1])
        
        input[bridge[0]].append(bridge[1])
        edge = nwedge

    if edge[1] in input[edge[0]]:
        input[edge[0]].remove(edge[1])

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

    e = find_cycle(test, 'a')

    while e is not None:
        # for node in test:
        #     print str(node) + '\t:\t' + str(test[node])
        
        print 'Unfolding: ' + str(e)
        unfold_cycle(test, e, 2)
        e = find_cycle(test, 'a')

    for node in test:
        print str(node) + '\t:\t' + str(test[node])

if __name__ == "__main__":
    main()